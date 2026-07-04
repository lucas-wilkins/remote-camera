from dataclasses import dataclass

@dataclass
class Settings:
    address: str = "127.0.0.1"
    control_port: int = 10001
    data_port: int = 10002
