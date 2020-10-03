import socket
from typing import Tuple
from socker_wrapper import SockerWrapper


class Client:
    def __init__(self, namenode_addr_tuple: Tuple[str, int]):
        self._namenode_addr_tuple = namenode_addr_tuple

    def run(self):
        cmd = input("$ ").strip()
        while cmd != "exit":
            try:
                cmd_args = cmd.split()
                if len(cmd_args) == 0:
                    print("Error: no command")
                elif cmd_args[0] in ("mkdir", "rmdir", "rm"):
                    if len(cmd_args) < 2:
                        print(f"Usage: {cmd_args[0]} <path>")
                    else:
                        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                            data = {
                                "message_type": "CLIENT",
                                "action_type": cmd_args[0],
                                "path": cmd_args[1]
                            }
                            wrapper = SockerWrapper(sock)
                            wrapper.connect(self._namenode_addr_tuple)
                            wrapper.send_msg_as_json(data)
                            resp = wrapper.recv_msg_as_json()
                            if resp["success"]:
                                print("Success")
                            else:
                                raise Exception(resp["message"])
                elif cmd_args[0] == "ins":  # insert file from outside of this filesystem (only way to create files)
                    if len(cmd_args) < 3:
                        print(f"Usage: {cmd_args[0]} <outside_path> <fs_path>")
                    else:
                        # read file data to ensure it exists
                        with open(cmd_args[1], "rb") as f:
                            file_bytes = f.read()
                        # contact namenode to create file, get datanodes to replicate data to
                        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                            data = {
                                "message_type": "CLIENT",
                                "action_type": "ins",
                                "path": cmd_args[2]
                            }
                            wrapper = SockerWrapper(sock)
                            wrapper.connect(self._namenode_addr_tuple)
                            wrapper.send_msg_as_json(data)
                            resp = wrapper.recv_msg_as_json()
                            if not resp["success"]:
                                raise Exception(resp["message"])
                        # send data to datanode
                        datanode_addr = tuple(resp["datanodes"][0])
                        data = {
                            "message_type": "WRITE_PIPELINE",
                            "datanodes": resp["datanodes"][1:],
                            "block_id": resp["block_id"]
                        }
                        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                            wrapper = SockerWrapper(sock)
                            wrapper.connect(datanode_addr)
                            wrapper.send_msg_as_json(data)
                            wrapper.send_msg(file_bytes)
                        print("Successfully written")
                elif cmd_args[0] == "ls":
                    if len(cmd_args) < 2:
                        print(f"Usage: {cmd_args[0]} <path>")
                    else:
                        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                            data = {
                                "message_type": "CLIENT",
                                "action_type": "ls",
                                "path": cmd_args[1]
                            }
                            wrapper = SockerWrapper(sock)
                            wrapper.connect(self._namenode_addr_tuple)
                            wrapper.send_msg_as_json(data)
                            resp = wrapper.recv_msg_as_json()
                            if resp["success"]:
                                for tup in resp["contents"]:
                                    file_name = tup[0]
                                    file_type = tup[1]
                                    print(f"  {file_type} {file_name}")
                            else:
                                raise Exception(resp["message"])
                elif cmd_args[0] == "cat":
                    if len(cmd_args) < 2:
                        print(f"Usage: {cmd_args[0]} <path>")
                    else:
                        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                            data = {
                                "message_type": "CLIENT",
                                "action_type": "cat",
                                "path": cmd_args[1]
                            }
                            wrapper = SockerWrapper(sock)
                            wrapper.connect(self._namenode_addr_tuple)
                            wrapper.send_msg_as_json(data)
                            resp = wrapper.recv_msg_as_json()
                            if not resp["success"]:
                                raise Exception(resp["message"])
                        datanode_addrs = resp["datanode_addrs"]
                        block_id = resp["block_id"]
                        got_file = False
                        file_bytes = None
                        while not got_file and len(datanode_addrs) > 0:
                            datanode = tuple(datanode_addrs.pop())
                            data = {
                                "message_type": "CLIENT_READ",
                                "block_id": block_id
                            }
                            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                                wrapper = SockerWrapper(sock)
                                try:
                                    wrapper.connect(datanode)
                                    wrapper.send_msg_as_json(data)
                                    resp = wrapper.recv_msg_as_json()
                                    if not resp["success"]:
                                        continue
                                    else:
                                        file_bytes = wrapper.recv_msg()
                                        got_file = True
                                        break
                                except ConnectionRefusedError:  # if datanode offline
                                    continue
                        if not got_file:
                            print("Error: couldn't locate file block")
                        else:
                            # only text files for now
                            file_text = str(file_bytes, "utf-8")
                            print(file_text)
                elif cmd_args[0] == "help":
                    txt = "mkdir <path>\nrmdir <path>\nrm <path>\nins <outside_path> <fs_path>\ncat <path>\nls <path>\nhelp\nexit"
                    print(txt)
                else:
                    print(f"Unknown command: {cmd_args[0]}")
            except Exception as e:
                print(str(e))
            finally:
                cmd = input("$ ").strip()


if __name__ == "__main__":
    NAMENODE_IP = "localhost"
    NAMENODE_PORT = 60420

    client = Client((NAMENODE_IP, NAMENODE_PORT))
    client.run()
