import struct
import threading
import socket

from PySide6.QtCore import Qt, QPoint
from PySide6.QtGui import QFont, QDoubleValidator, QIntValidator
from PySide6.QtWidgets import QWidget, QPushButton, QComboBox, QLineEdit, QVBoxLayout, QSpacerItem, QHBoxLayout, \
    QApplication, QSizePolicy, QTextEdit

from client.read_from_remote import ImageData
from settings import Settings
settings = Settings()


class ControlClient:
    def __init__(self, parent: "ControlWindow"):
        self.parent = parent
        parent.client_message("Connecting control client")
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((settings.address, settings.control_port))


    def send(self, data: bytes):

        try:
            self.socket.sendall(data)

        except:
            self.socket.close()


    def disconnect(self):
        self.socket.close()

    def status(self):
        self.send(bytes([0]))

    def capture(self, index: int):
        b = bytes([1]) + int(index).to_bytes(8, 'little')
        self.send(b)

    def exposure(self, time: int):
        b = bytes([2]) + int(time).to_bytes(8, 'little')
        self.send(b)

    def gain(self, value: float):
        b = bytes([3]) + struct.pack('f', value)
        self.send(b)

    def disconnect(self):
        try:
            self.parent.client_message("Disconnecting control client")
            self.socket.close()
        except Exception as e:
            print(e)



class ControlWindow(QWidget):
    def __init__(self, address:str = settings.address, parent=None):
        super().__init__(parent)

        self.setWindowTitle("Camera Control")

        main_layout = QHBoxLayout()
        self.setLayout(main_layout)

        button_panel = QWidget()
        button_panel_layout = QVBoxLayout()
        button_panel.setLayout(button_panel_layout)

        connect_control = QPushButton("Connect")
        connect_control.clicked.connect(self.connect)
        button_panel_layout.addWidget(connect_control)

        disconnect_control = QPushButton("Disconnect")
        disconnect_control.clicked.connect(self.disconnect)
        button_panel_layout.addWidget(disconnect_control)

        button_panel_layout.addSpacerItem(QSpacerItem(20, 20, QSizePolicy.Policy.Minimum , QSizePolicy.Policy.Minimum))

        get_status = QPushButton("Status")
        get_status.clicked.connect(self.status)
        button_panel_layout.addWidget(get_status)

        get_image = QPushButton("Image")
        get_image.clicked.connect(self.capture)
        button_panel_layout.addWidget(get_image)

        button_panel_layout.addSpacerItem(QSpacerItem(20, 20, QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Minimum))

        self.exposure_value = QLineEdit("10000")
        self.exposure_value.setValidator(QIntValidator(0, 1_000_000, self.exposure_value))
        button_panel_layout.addWidget(self.exposure_value)

        set_exposure = QPushButton("Set Exposure")
        set_exposure.clicked.connect(self.exposure)
        button_panel_layout.addWidget(set_exposure)

        button_panel_layout.addSpacerItem(QSpacerItem(20, 20, QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Minimum))

        self.gain_value = QLineEdit("2.0")
        gain_validator = QDoubleValidator(0.0, 100.0, 3, self.gain_value)
        gain_validator.setNotation(QDoubleValidator.StandardNotation)
        self.gain_value.setValidator(gain_validator)
        button_panel_layout.addWidget(self.gain_value)

        set_gain = QPushButton("Set Gain")
        set_gain.clicked.connect(self.gain)
        button_panel_layout.addWidget(set_gain)

        main_layout.addWidget(button_panel)

        self.log_area = QTextEdit("")

        self.log_area.setReadOnly(True)
        self.log_area.setLineWrapMode(
            QTextEdit.LineWrapMode.NoWrap
        )

        # Keep only the most recent 10,000 lines
        self.log_area.document().setMaximumBlockCount(10_000)

        # Monospace font
        self.log_area.setFont(QFont("Monospace"))


        main_layout.addWidget(self.log_area)

        # for i in range(20):
        #     self.client_message(f"Test {i}")
        #     self.server_message(f"Test {i}")


        # Networking stuff
        self.stay_connected = True

        self.control_client: ControlClient | None = None
        self.capture_index = 0



    def client_message(self, msg: str):
        self.log_area.append(f'<span style="color: pink;">[CLIENT] {msg}</span>')

    def server_message(self, msg: str):
        self.log_area.append(f'<span style="color: purple;">[SERVER] {msg}</span>')

    @property
    def connected(self) -> bool:
        return self.control_client is not None and self.stay_connected


    def connect(self):
        if self.connected:
            self.client_message("Already Connected")
            return

        def message_client_fun():

            self.client_message("Connecting message client")

            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.connect((settings.address, settings.message_port))
                    s.settimeout(0.1)

                    buffer = b''

                    while self.stay_connected:
                        try:
                            data = s.recv(1024)

                            if len(data) == 0:
                                break

                            buffer += data

                            while b"\n" in buffer:
                                line, buffer = buffer.split(b"\n", 1)
                                self.server_message(line.decode())

                        except socket.timeout:
                            continue

                    self.stay_connected = False

                    self.client_message("Disconnected message client")

            except Exception as e:
                print(e)



        self.stay_connected = True

        self.message_thread = threading.Thread(target=message_client_fun)
        self.message_thread.start()

        self.control_client = ControlClient(self)



    def disconnect(self):
        if not self.connected:
            self.client_message("Already Disconnected")

        if self.control_client is not None:
            self.control_client.disconnect()

        self.stay_connected = False

        self.control_client = None




    def status(self):
        if not self.connected:
            self.client_message("Not Connected")
            return

        self.control_client.status()

    def capture(self):
        if not self.connected:
            self.client_message("Not Connected")
            return

        self.control_client.capture(self.capture_index)
        self.capture_index += 1

    def exposure(self):
        if not self.connected:
            self.client_message("Not Connected")
            return

        try:
            self.control_client.exposure(int(self.exposure_value.text()))
        except Exception as e:
            print(e)

    def gain(self):
        if not self.connected:
            self.client_message("Not Connected")
            return

        try:
            self.control_client.gain(float(self.gain_value.text()))
        except Exception as e:
            print(e)

class DataWindow(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.setWindowTitle("Data Server")

        layout = QVBoxLayout()

        connect = QPushButton("Connect")
        connect.clicked.connect(self.connect)

        disconnect = QPushButton("Disconnect")
        disconnect.clicked.connect(self.disconnect)

        layout.addWidget(connect)
        layout.addWidget(disconnect)

        self.setLayout(layout)



        self.connected = False
        self.message_thread = None


    def message(self, msg):
        print(msg)

    def connect(self):

        if self.connected:
            self.message("Already Connected")
            return

        def message_client_fun():

            self.message("Connecting message client")

            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.connect((settings.address, settings.data_port))
                    s.settimeout(0.1)

                    buffer = b''

                    self.connected = True

                    while self.stay_connected:
                        try:
                            image_data = ImageData.receive(s)
                            self.message(repr(image_data.header))

                        except socket.timeout:
                            continue

                    self.stay_connected = False

                    self.message("Disconnected message client")

            except Exception as e:
                print(e)

            finally:
                self.connected = False

        self.stay_connected = True

        self.message_thread = threading.Thread(target=message_client_fun)
        self.message_thread.start()

    def disconnect(self):
        if not self.connected:
            self.message("Already Disconnected")

        self.stay_connected = False


if __name__ == "__main__":
    app = QApplication()
    ctrl = ControlWindow()
    data = DataWindow()


    ctrl.show()
    data.show()
    data.move(QPoint(500, 300))

    app.exec()