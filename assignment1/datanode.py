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
        # get existing blocks in blocks dir, else it'll just be empty
        self._block_ids = os.listdir(self._block_filepath_prefix)

    def get_block_ids(self):
        # return list of block IDs
        return self._block_ids

    def _get_block_filepath(self, block_id: str):
        # physical path to block
        return os.path.join(self._block_filepath_prefix, block_id)

    def has_block(self, block_id: str):
        return block_id in self._block_ids

    def read_block(self, block_id: str):
        # return bytes of block
        with open(self._get_block_filepath(block_id), "rb") as f:
            return f.read()

    def add_block(self, block_id: str, raw_data: bytes):
        # write block to disk and append id to list
        with open(self._get_block_filepath(block_id), "wb") as f:
            f.write(raw_data)
        self._block_ids.append(block_id)


class DatanodeServer:
    HEARTBEAT_FREQUENCY = 10  # seconds
    CONFIG_FILENAME = "hdfs_config.json"

    def __init__(self, server_addr_tuple: Tuple[str, int], root_filepath: str):
        self._root_filepath = root_filepath
        self._config = self._load_config()
        self._block_manager = BlockManager(self._root_filepath)

        self._namenode_addr_tuple = tuple(self._config["namenode_addr_tuple"])
        self._server_addr_tuple = server_addr_tuple

        # for listening (incoming connections from hdfs clients or other datanodes)
        self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def run(self):
        # attempt handshake with namenode
        if not self._handshake():
            print("Failed handshake")
            sys.exit(1)
        print("Successful handshake")
        self._persist_config()
        # spin up separate thread to do heartbeats
        threading.Thread(target=self._heartbeat).start()
        # main thread will perform listen for incoming connections
        self._listen()

    def _load_config(self):
        # try load config if it exists, else set defaults
        try:
            p = os.path.join(self._root_filepath, DatanodeServer.CONFIG_FILENAME)
            with open(p, "r") as f:
                config = json.load(f)
                print(f"Loading config at: {p}")
        except FileNotFoundError:
            print("No config found")
            config = {
                "software_version": "1.0.0",
                "datanode_id": str(uuid.uuid4()),  # generate new one
                "namespace_id": None,
                "namenode_addr_tuple": ("localhost", 60420)
            }
        return config

    def _persist_config(self):
        # in case need to update config from namenode response
        p = os.path.join(self._root_filepath, DatanodeServer.CONFIG_FILENAME)
        print(f"Persisting config at: {p}")
        with open(p, "w") as f:
            json.dump(self._config, f)

    def _handshake(self):
        # send payload, see if namenode accepts (proceed) or rejects (shutdown)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            data = {
                "message_type": "DATANODE_HANDSHAKE",
                "software_version": self._config["software_version"],  # ensure software versions match
                "datanode_id": self._config["datanode_id"],  # for namenode to identify datanode
                "namespace_id": self._config["namespace_id"],  # to ensure in same cluster as namenode
                "address_tuple": self._server_addr_tuple,  # let namenode know address of this datanode
                "handshake": None
            }
            wrapper = SockerWrapper(sock)
            wrapper.connect(self._namenode_addr_tuple)
            wrapper.send_msg_as_json(data)
            resp = wrapper.recv_msg_as_json()
            self._config["namespace_id"] = resp["namespace_id"]
            return resp["handshake"]

    def _listen(self):
        self._server_socket.bind(self._server_addr_tuple)
        # accept up to 5 connections at a time
        self._server_socket.listen(5)
        while True:  # accepting connections from hdfs client or other datanodes
            client, _ = self._server_socket.accept()
            client.settimeout(60)  # 60 second timeout for the blocking ops
            # spin up new thread to handle this new connection
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
            # indicate if block found or not
            data = {
                "success": self._block_manager.has_block(initial_msg["block_id"])
            }
            wrapper.send_msg_as_json(data)
            # proceed to send block raw bytes if found
            if data["success"]:
                wrapper.send_msg(self._block_manager.read_block(initial_msg["block_id"]))
        else:
            print(f"UNKNOWN MESSAGE TYPE FOR MESSAGE: {str(initial_msg)}")
        client.close()

    def _heartbeat(self):
        while True:
            # send heartbeat containing block report to namenode on regular frequency
            time.sleep(DatanodeServer.HEARTBEAT_FREQUENCY)
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                data = {
                    "message_type": "DATANODE_HEARTBEAT",
                    "datanode_id": self._config["datanode_id"],
                    "block_report": self._block_manager.get_block_ids()
                }
                wrapper = SockerWrapper(sock)
                wrapper.connect(self._namenode_addr_tuple)
                wrapper.send_msg_as_json(data)
                print("Sent heartbeat")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} root_filepath unique_port")
        sys.exit(1)
    root_filepath, port = sys.argv[1], int(sys.argv[2])

    server = DatanodeServer(("localhost", port), root_filepath)
    server.run()
