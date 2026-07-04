import socket
import time

from messsagetypes import MessageType

from settings import Settings
import threading
import struct

settings = Settings()

def data_loop():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((settings.address, settings.data_port))

        while True:
            data = s.recv(1024)
            if len(data) > 0:
                print(data.decode())

            time.sleep(0.01)

t = threading.Thread(target=data_loop)
t.start()

test_exposure = 12345
test_gain = 2.3

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((settings.address, settings.control_port))
    while True:

        # Get the message type

        print("> ", end="")
        string = input()

        try:
            n = int(string)
        except:
            print("Not a number")
            continue

        b = bytes([n])

        match n:
            case 2:
                b = b + int(test_exposure).to_bytes(8, 'little')
            case 3:
                b = b + struct.pack('f', test_gain)
        # Send the number

        print(b)

        s.sendall(b)

        # Get server response and print

        data = s.recv(1024)
        print(data.decode())