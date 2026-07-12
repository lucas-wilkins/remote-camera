import numpy as np

def load_raw_from_file(filename: str):
    return read_raw(np.fromfile(filename, dtype=np.uint8))


def read_raw(data: bytes, width: int = 4056, height: int = 3040):

    b0 = data[0::3].astype(np.uint16)
    b1 = data[1::3].astype(np.uint16)
    b2 = data[2::3].astype(np.uint16)

    print(width*height*3/2)
    print(len(b0)+len(b1)+len(b2))
    print(width * height * 3 / 2)
    print(len(b0) + len(b1) + len(b2) - width*height*3/2)

    print(len(b0))
    print(len(b1))
    print(len(b2))

    p0 = (b0 << 4) | (b2 & 0x0F)
    p1 = (b1 << 4) | (b2 >> 4)

    pixels = np.empty(p0.size + p1.size, dtype=np.uint16)
    pixels[0::2] = p0
    pixels[1::2] = p1

    image = pixels.reshape(height, width)

    return image

if __name__ == "__main__":
    load_raw_from_file("image.bin")