""" Just listen on the ports """

import socket
import threading
import time

from settings import Settings

settings = Settings()

HOST = "127.0.0.1"

def listen_server(port):
    def fun():
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((HOST, port))
            s.listen()

            print(f"Listening on {HOST}:{port}")

            while True:
                conn, addr = s.accept()
                print(f"Connection from {addr}")

                with conn:
                    while data := conn.recv(4096):
                        print(f"Port {port}", data)
                        conn.sendall(data)  # echo back
    return fun

t = threading.Thread(target=listen_server(settings.control_port))
t.start()
time.sleep(0.1)

t = threading.Thread(target=listen_server(settings.data_port))
t.start()

while True:
    time.sleep(1)