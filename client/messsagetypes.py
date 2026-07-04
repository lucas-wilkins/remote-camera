from enum import Enum


class MessageType(Enum):
    STATUS = 0
    CAPTURE = 1
    SET_EXPOSURE = 2
    SET_GAIN = 3