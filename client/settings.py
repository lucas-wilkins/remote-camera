from dataclasses import dataclass

@dataclass
class Settings:
    address: str = "127.0.0.1"
    # address: str = "192.168.2.102"
    control_port: int = 10001
    message_port: int = 10002
    data_port: int = 10003
