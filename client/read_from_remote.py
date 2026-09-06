import socket
import struct
import time
from dataclasses import dataclass

import numpy as np

from client.read_raw import convert_raw


def recv_exact(sock, n):
    data = bytearray(n)
    view = memoryview(data)

    pos = 0
    while pos < n:
        received = sock.recv_into(view[pos:])
        if received == 0:
            raise ConnectionError("Connection closed")
        pos += received

    return data


@dataclass
class FrameInfo:
    image_id: int
    timestamp: int
    bytesused: int
    frameDuration: int
    exposure: int

    _STRUCT = struct.Struct("<IQIqq")

    @classmethod
    def from_bytes(cls, data: bytes) -> "FrameInfo":
        return cls(*cls._STRUCT.unpack(data))

    @classmethod
    def size(cls):
        return cls._STRUCT.size

class ImageData:
    def __init__(self,
                 header: FrameInfo,
                 raw_bytes: bytes,
                 transfer_time: float):

        self.header = header
        self.transfer_time = transfer_time

        numpy_data = np.frombuffer(raw_bytes, dtype=np.uint8)

        self.raw_image = convert_raw(numpy_data)

    def matplotlib_show(self):
        import matplotlib.pyplot as plt

        plt.imshow(self.raw_image)

        plt.show()


    @staticmethod
    def receive(sock):
        tic = time.time()
        header_data = recv_exact(sock, FrameInfo.size())
        header = FrameInfo.from_bytes(header_data)

        data_section = recv_exact(sock, header.bytesused)
        transfer_time = time.time() - tic

        return ImageData(header, data_section, transfer_time)