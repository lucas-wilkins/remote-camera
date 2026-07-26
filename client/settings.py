from dataclasses import dataclass

@dataclass
class Settings:
    address: str = "192.168.2.102"
    control_port: int = 10001
    data_port: int = 10002
