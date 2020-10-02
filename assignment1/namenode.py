import os
import sys
from socker_wrapper import SockerWrapper
import threading
import socket
from typing import Tuple, List
import pickle
import uuid
import random


SOFTWARE_VERSION = "1.0.0"
NAMESPACE_ID = "pythonhdfs3143"
REPLICATION_FACTOR = 2


class Inode:  # can be a dir or a file
    def __init__(self, name: str, is_dir: bool, block_id=None):
        self._is_dir = is_dir
        self._name = name
        # if is dir, which inodes it contains
        # index = index in inode table
        self._inodes = []  # (index, name) tuples
        # if not dir (is file)
        self._block_id = block_id

    def is_dir(self):
        return self._is_dir

    def get_name(self):
        return self._name

    def get_inodes(self):
        return self._inodes

    def get_block_id(self):
        return self._block_id

    def add_inode(self, index: int, name: str):
        # add inode as tuple: index (in inode table) and name of the inode
        if not self._is_dir:
            raise ValueError("Not a directory")
        t = (index, name)
        if t in self._inodes:
            raise ValueError("Already in directory")
        self._inodes.append(t)

    def get_inode_index(self, name: str):
        # return inode index of specified dir (name) if in this dir
        if not self._is_dir:
            raise ValueError("Not a directory")
        for t in self._inodes:
            if t[1] == name:
                return t[0]
        raise ValueError(f"Not found: {name}")

    def rm_inode(self, index: int):
        if not self._is_dir:
            raise ValueError("Not a directory")
        self._inodes = [x for x in self._inodes if x[0] != index]


class FileSystem:
    # support this many inodes at most
    INODE_BITMAP_SIZE = 4096

    def __init__(self, fsimage_path: str):
        # fill with garbage
        self._inode_table = [Inode("", False)] * FileSystem.INODE_BITMAP_SIZE
        # use to identify which slots in _inode_table are free
        self._inode_bitmap = [False] * FileSystem.INODE_BITMAP_SIZE
        self._path = fsimage_path

        # root is first element
        self._add_inode(Inode("/", True))

    def _add_inode(self, inode: Inode):
        counter = 0
        # find first empty slot in bitmap
        while self._inode_bitmap[counter] and counter < FileSystem.INODE_BITMAP_SIZE:
            counter += 1
        if counter >= FileSystem.INODE_BITMAP_SIZE:
            raise ValueError("Cannot allocate inode: bitmap full")
        self._inode_table[counter] = inode
        self._inode_bitmap[counter] = True
        return counter

    def _get_root_inode(self):
        # root is always first
        return self._inode_table[0]

    def _get_inode_from_dir_list(self, dir_list: List[str], expect_dir: bool):
        if len(dir_list) == 0:  # root
            return self._get_root_inode()

        # start from root node
        initial_inode = self._get_root_inode()
        while len(dir_list) > 0:
            # look for dir name (from dir_list) in dirs that current dir (initial_inode) contains
            d_name = dir_list.pop(0)
            index = initial_inode.get_inode_index(d_name)
            initial_inode = self._inode_table[index]
            # found the specified name, check if dir or not (if we expect a dir)
            if expect_dir and not initial_inode.is_dir():
                raise ValueError(f"Not a directory: {initial_inode.get_name()}")
        return initial_inode

    def _verify_path(self, path: str):
        if path[0] != "/":
            raise ValueError("Must specify absolute path")
        if path[-1] == "/" and len(path) > 1:
            raise ValueError("Trailing slash")

    # return block id of inode specified by path if exists
    def get_block_id_from_path(self, path: str):
        self._verify_path(path)
        dirs = path.strip("/").split("/")
        inode = self._get_inode_from_dir_list(dirs, False)
        if inode.is_dir():
            raise ValueError(f"Is a directory: {path}")
        return inode.get_block_id()

    def get_dir_contents(self, path: str):
        self._verify_path(path)
        dirs = path.strip("/").split("/")
        # get rid of empty strings
        dirs = [x for x in dirs if len(x) > 0]
        inode = self._get_inode_from_dir_list(dirs, True)
        # tuple: (name, type (f or d))
        contents = []
        for tup in inode.get_inodes():
            index = tup[0]
            name = tup[1]
            inode_type = "d" if self._inode_table[index].is_dir() else "f"
            contents.append((name, inode_type))
        return contents

    def mkdir(self, path: str):
        self._verify_path(path)
        dirs = path.strip("/").split("/")
        existing_dirs, new_dir = dirs[:-1], dirs[-1]
        # check the provided path and get dir at the end
        inode = self._get_inode_from_dir_list(existing_dirs, True)
        # add this new dir
        i = self._add_inode(Inode(new_dir, True))
        inode.add_inode(i, new_dir)
        print(f"mkdir: {path}")
        self._persist_fsimage()

    def rmdir(self, path: str):
        self._verify_path(path)
        dirs = path.strip("/").split("/")
        existing_dirs, rm_dir = dirs[:-1], dirs[-1]
        # check the provided path and get dir at the end
        inode = self._get_inode_from_dir_list(existing_dirs, True)
        # get dir to be removed
        i = inode.get_inode_index(rm_dir)
        rm_inode = self._inode_table[i]
        if not rm_inode.is_dir():
            raise ValueError(f"Not a directory: {rm_inode.get_name()}")
        inode.rm_inode(i)
        # delete this dir recursively
        self._rm(rm_inode, i)
        print(f"rmdir: {path}")
        self._persist_fsimage()

    def _rm(self, inode: Inode, index: int):
        # helper for rmdir
        # to remove, mark them as unused in the bitmap
        self._inode_bitmap[index] = False
        for tup in inode.get_inodes():  # if rm file, this will return empty list hence terminate here
            new_index, _ = tup
            self._rm(self._inode_table[new_index], new_index)

    def mkfile(self, path: str):
        self._verify_path(path)
        dirs = path.strip("/").split("/")
        existing_dirs, filename = dirs[:-1], dirs[-1]
        # check the provided path and get dir at the end
        inode = self._get_inode_from_dir_list(existing_dirs, True)
        # add this new dir
        new_inode = Inode(filename, False, str(uuid.uuid4()))
        i = self._add_inode(new_inode)
        inode.add_inode(i, filename)
        print(f"mkfile: {path}")
        self._persist_fsimage()
        return new_inode.get_block_id()

    def rmfile(self, path: str):
        self._verify_path(path)
        dirs = path.strip("/").split("/")
        existing_dirs, rmfile = dirs[:-1], dirs[-1]
        # check the provided path and get file at the end
        inode = self._get_inode_from_dir_list(existing_dirs, False)
        # get dir to be removed
        i = inode.get_inode_index(rmfile)
        rm_inode = self._inode_table[i]
        if rm_inode.is_dir():
            raise ValueError(f"Not a file: {rm_inode.get_name()}")
        inode.rm_inode(i)
        # delete this file
        self._rm(rm_inode, i)
        print(f"rmfile: {path}")
        self._persist_fsimage()
        return rm_inode.get_block_id()

    def _persist_fsimage(self):
        with open(self._path, "wb") as f:
            pickle.dump(self, f)
        print("Persisted FSImage")


class Server:
    def __init__(self, addr_tuple: Tuple[str, int], fsimage: FileSystem):
        self._addr_tuple = addr_tuple
        self._fsimage = fsimage
        self._fsimage_write_lock = threading.Lock()
        # map datanode id to their address tuple
        self._datanodes = {}
        # map datanode id to list of its block ids
        self._blocks = {}

        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def run(self):
        self._socket.bind(self._addr_tuple)
        self._socket.listen(5)
        while True:
            client, address = self._socket.accept()
            client.settimeout(60)
            threading.Thread(target=self._client_activity, args=(client,)).start()

    def _client_activity(self, client: socket.socket):
        wrapper = SockerWrapper(client)
        initial_msg = wrapper.recv_msg_as_json()

        if initial_msg["message_type"] == "DATANODE_HANDSHAKE":  # datanode requesting handshake
            print("Handshake request")
            # check if software versions match
            initial_msg["handshake"] = initial_msg["software_version"] == SOFTWARE_VERSION
            if initial_msg["namespace_id"] is None:  # new datanode, add this this cluster
                initial_msg["namespace_id"] = NAMESPACE_ID
                initial_msg["handshake"] = False or initial_msg["handshake"]
            elif initial_msg["namespace_id"] != NAMESPACE_ID:  # datanode in wrong cluster
                initial_msg["handshake"] = False
            else:  # datanode in correct cluster
                initial_msg["handshake"] = True and initial_msg["handshake"]

            if initial_msg["handshake"]:  # if successful, add to list of datanodes
                self._datanodes[initial_msg["datanode_id"]] = initial_msg["address_tuple"]
            # respond to handshake
            wrapper.send_msg_as_json(initial_msg)
        elif initial_msg["message_type"] == "DATANODE_HEARTBEAT":  # update datanodes list of blocks
            print("Datanode heartbeat")
            self._blocks[initial_msg["datanode_id"]] = initial_msg["block_report"]
        elif initial_msg["message_type"] == "CLIENT":  # requests from hdfs client
            print("Client activity")
            # these activities are very similar
            if initial_msg["action_type"] in ("mkdir", "rmdir", "rm"):
                path = initial_msg["path"]
                data = {}
                # lock to prevent any other thread also modifying fsimage at same time
                with self._fsimage_write_lock:
                    try:
                        if initial_msg["action_type"] == "mkdir":
                            # make a directory
                            self._fsimage.mkdir(path)
                        elif initial_msg["action_type"] == "rmdir":
                            # remove a directory (recursively)
                            self._fsimage.rmdir(path)
                        elif initial_msg["action_type"] == "rm":
                            # remove a file (inode)
                            # won't remove actual blocks from datanodes, though ideally should
                            self._fsimage.rmfile(path)
                        data["success"] = True
                    except Exception as e:
                        # some sort of error, inform client
                        data["success"] = False
                        data["message"] = str(e)
                wrapper.send_msg_as_json(data)
            elif initial_msg["action_type"] == "ins":
                # client wants to creat file
                path = initial_msg["path"]
                data = {}
                with self._fsimage_write_lock:
                    try:
                        # add the file inode
                        block_id = self._fsimage.mkfile(path)
                        data["success"] = True
                        # choose 2 random datanodes to replicate to, send their address tuples
                        data["datanodes"] = random.sample(list(self._datanodes.values()), REPLICATION_FACTOR)
                        data["block_id"] = block_id
                    except Exception as e:
                        data["success"] = False
                        data["message"] = str(e)
                wrapper.send_msg_as_json(data)
            elif initial_msg["action_type"] == "cat":
                # client wants a file data
                path = initial_msg["path"]
                data = {}
                try:
                    # locate the block id
                    block_id = self._fsimage.get_block_id_from_path(path)
                    datanode_found = False
                    # find all datanodes that hold that specific block
                    data["datanode_addrs"] = []
                    for datanode_id in self._blocks:
                        if block_id in self._blocks[datanode_id]:
                            data["datanode_addrs"].append(self._datanodes[datanode_id])
                            datanode_found = True
                    if not datanode_found:
                        raise Exception(f"Datanode not found for block {block_id}")
                    data["success"] = True
                    data["block_id"] = block_id
                except Exception as e:
                    data["success"] = False
                    data["message"] = str(e)
                wrapper.send_msg_as_json(data)
            elif initial_msg["action_type"] == "ls":
                # list contents of a dir
                path = initial_msg["path"]
                data = {}
                try:
                    # list of tuple: (name, type (f or d))
                    data["contents"] = self._fsimage.get_dir_contents(path)
                    data["success"] = True
                except Exception as e:
                    data["success"] = False
                    data["message"] = str(e)
                wrapper.send_msg_as_json(data)
        else:
            print(f"UNKNOWN MESSAGE TYPE FOR MESSAGE: {str(initial_msg)}")

        client.close()


if __name__ == "__main__":
    NAMENODE_IP = "localhost"
    NAMENODE_PORT = 60420

    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} root_filepath")
        sys.exit(1)
    root_filepath = sys.argv[1]
    fsimage_filepath = os.path.join(root_filepath, "fsimage")
    # read fsimage
    try:
        with open(fsimage_filepath, "rb") as f:
            fsimage = pickle.load(f)
            print(f"Found fsimage at {fsimage_filepath}")
    except FileNotFoundError:
        fsimage = FileSystem(fsimage_filepath)
        print(f"Created default new fsimage at {fsimage_filepath}")

    # start listening for connections
    server = Server((NAMENODE_IP, NAMENODE_PORT), fsimage)
    server.run()
