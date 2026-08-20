""" Just listen on the ports """

import socket
import threading
import time

from settings import Settings
from messsagetypes import MessageType

settings = Settings()

HOST = "127.0.0.1"

class SendServer:
    def __init__(self, port):
        self.port = port

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((HOST, port))
        self.sock.listen()

        print(f"Listening on {HOST}:{port}")

        self.event = threading.Event()
        self.msg: str | None = None

        t = threading.Thread(target=self.run)
        t.start()

    def send(self, msg: str):
        self.msg = msg
        self.event.set()

    def run(self):

        while True:
            conn, addr = self.sock.accept()
            print(f"Connection from {addr[0]}:{self.port} ({addr[1]})\n", end="")


            with conn:
                connected = True
                while connected:

                    self.event.wait(timeout=0.1)
                    self.event.clear()
                    if self.msg is not None:
                        conn.sendall(self.msg.encode())
                        self.msg = None

            print(f"Client disconnected {addr[0]}:{self.port} ({addr[1]})\n", end="")


def listen_server(port, message_server: SendServer, data_server: SendServer):

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, port))
        s.listen()

        print(f"Listening on {HOST}:{port}\n", end="")

        while True:
            conn, addr = s.accept()
            print(f"Connection from {addr[0]}:{port} ({addr[1]})\n", end="")

            with conn:
                while data := conn.recv(4096):
                    message_server.send(f"Received {data}\n")

                    if data[0] == MessageType.CAPTURE.value:
                        data_server.send("Dummy capture data\n")

            print(f"Client disconnected {addr[0]}:{port} ({addr[1]})\n", end="")

message_server = SendServer(settings.message_port)
data_server = SendServer(settings.data_port)

listen_server(settings.control_port, message_server, data_server)