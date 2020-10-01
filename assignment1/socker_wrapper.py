import socket
import struct
from typing import Tuple
import json


# wrap socket to be able to prepend msg len to message sent via socket
class SockerWrapper:
    def __init__(self, socket: socket.socket):
        self._socket = socket

    def connect(self, addr_tuple: Tuple[str, int]):
        self._socket.connect(addr_tuple)

    # from https://stackoverflow.com/questions/17667903/python-socket-receive-large-amount-of-data
    def send_msg(self, msg: bytes):
        # prepend with 4 byte unsigned int (big endian) to indicate message length
        self._socket.sendall(struct.pack(">I", len(msg)) + msg)

    def send_msg_as_json(self, msg: dict):  # convert to json then to bytes
        # send bytes from dict
        self.send_msg(bytes(json.dumps(msg), "utf-8"))

    def recv_msg(self):
        # get message length
        raw_msglen = self._recvall(4)
        if not raw_msglen:
            return None
        # unpack into int
        msglen = struct.unpack('>I', raw_msglen)[0]
        # get actual message data
        return self._recvall(msglen)

    def recv_msg_as_json(self):  # convert to dict from json bytes
        # returns dict
        return json.loads(str(self.recv_msg(), "utf-8"))

    def _recvall(self, n: int):
        # recv n bytes or return None if EOF is hit
        data = bytearray()
        while len(data) < n:
            packet = self._socket.recv(n - len(data))
            if not packet:
                return None
            data.extend(packet)
        return data
