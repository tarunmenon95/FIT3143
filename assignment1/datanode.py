import socket
import sys
import json
import uuid
import os
import threading
from socker_wrapper import SockerWrapper
from typing import Tuple
import time
from pathlib import Path


class BlockManager:
    def __init__(self, root_filepath: str):
        self._block_filepath_prefix = os.path.join(root_filepath, "blocks")
        # create dir if not exists
        Path(self._block_filepath_prefix).mkdir(parents=True, exist_ok=True)
        # list of block IDs
        # get existing blocks, else it'll just be empty
        self._blocks = os.listdir(self._block_filepath_prefix)

    def get_block_report(self):
        # return list of block IDs
        return self._blocks

    def _get_block_filepath(self, block_id: str):
        return os.path.join(self._block_filepath_prefix, block_id)

    def has_block(self, block_id: str):
        return block_id in self._blocks

    def read_block(self, block_id: str):
        with open(self._get_block_filepath(block_id), "rb") as f:
            return f.read()

    def add_block(self, block_id: str, raw_data: bytes):
        with open(self._get_block_filepath(block_id), "wb") as f:
            f.write(raw_data)
        self._blocks.append(block_id)


class Server:
    HEARTBEAT_FREQUENCY = 10  # seconds

    def __init__(self, addr_tuple: Tuple[str, int], namenode_addr_tuple: Tuple[str, int], block_manager: BlockManager):
        self._addr_tuple = addr_tuple
        self._namenode_addr_tuple = namenode_addr_tuple
        self._block_manager = block_manager

        self._client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_socket.bind(self._addr_tuple)

    def run(self):
        threading.Thread(target=self._heartbeat).start()
        self._listen()

    def _listen(self):
        self._server_socket.listen(5)
        while True:  # accepting connections from hdfs client or other datanodes
            client, address = self._server_socket.accept()
            client.settimeout(60)
            threading.Thread(target=self._client_activity, args=(client,)).start()

    def _client_activity(self, client: socket.socket):
        wrapper = SockerWrapper(client)
        initial_msg = wrapper.recv_msg_as_json()

        if initial_msg["message_type"] == "WRITE_PIPELINE":
            # connection from client/datanode to set up pipeline to write
            print("Request for pipeline")
            file_bytes = wrapper.recv_msg()
            block_id = initial_msg["block_id"]
            datanodes = initial_msg["datanodes"]
            self._block_manager.add_block(block_id, file_bytes)
            print(f"{block_id} written")
            if len(datanodes) != 0:
                next_datanode, datanodes = initial_msg["datanodes"][0], initial_msg["datanodes"][1:]
                initial_msg["datanodes"] = datanodes
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                    wrapper = SockerWrapper(sock)
                    wrapper.connect(tuple(next_datanode))
                    wrapper.send_msg_as_json(initial_msg)
                    wrapper.send_msg(file_bytes)
                    print("Send to next datanode")
        elif initial_msg["message_type"] == "CLIENT_READ":
            # client requesting a block
            print("Request for read")
            data = {}
            data["success"] = self._block_manager.has_block(initial_msg["block_id"])
            wrapper.send_msg_as_json(data)
            if data["success"]:
                wrapper.send_msg(self._block_manager.read_block(initial_msg["block_id"]))
        else:
            print(f"UNKNOWN MESSAGE TYPE FOR MESSAGE: {str(initial_msg)}")

        client.close()

    def _heartbeat(self):
        while True:
            # send heartbeat containing block report to namenode on regular frequency
            time.sleep(Server.HEARTBEAT_FREQUENCY)
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                data = {
                    "message_type": "DATANODE_HEARTBEAT",
                    "datanode_id": config["datanode_id"],
                    "block_report": self._block_manager.get_block_report()
                }
                wrapper = SockerWrapper(sock)
                wrapper.connect(self._namenode_addr_tuple)
                wrapper.send_msg_as_json(data)
                print("Sent heartbeat")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} root_filepath unique_port")
        sys.exit(1)
    root_filepath = sys.argv[1]
    port = int(sys.argv[2])
    config_filename = "hdfs_config.json"
    # try load config if it exists, else get default
    try:
        p = os.path.join(root_filepath, config_filename)
        with open(p, "r") as f:
            config = json.load(f)
            print(f"Loading config at: {p}")
    except FileNotFoundError:
        print("No config found, getting default")
        config = {
            "software_version": "1.0.0",
            "datanode_id": str(uuid.uuid4()),
            "namespace_id": None,
            "physical_root_filepath": root_filepath,
            "namenode_addr_tuple": ("localhost", 60420),
            "datanode_addr_tuple": ("localhost", port)
        }
    print(config)

    # create block manager
    block_manager = BlockManager(root_filepath)

    # do handshake
    # send payload, see if namenode accepts (proceed) or rejects (shutdown)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        data = {
            "message_type": "DATANODE_HANDSHAKE",
            "software_version": config["software_version"],
            "datanode_id": config["datanode_id"],
            "namespace_id": config["namespace_id"],
            "address_tuple": config["datanode_addr_tuple"],
            "handshake": None
        }

        wrapper = SockerWrapper(sock)
        wrapper.connect(tuple(config["namenode_addr_tuple"]))
        wrapper.send_msg_as_json(data)
        resp = wrapper.recv_msg_as_json()
        if resp["handshake"]:
            print("Successful handshake")
            config["namespace_id"] = resp["namespace_id"]
        else:
            print("Failed handshake")
            sys.exit(1)

    # in case need to update config from namenode response
    p = os.path.join(config["physical_root_filepath"], config_filename)
    print(f"Persisting config at: {p}")
    print(config)
    with open(p, "w") as f:
        json.dump(config, f)

    server = Server(tuple(config["datanode_addr_tuple"]), tuple(config["namenode_addr_tuple"]), block_manager)
    server.run()
