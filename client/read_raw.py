import numpy as np

def load_raw_from_file(filename: str):
    return read_raw(np.fromfile(filename, dtype=np.uint8))


def read_raw(data: np.ndarray, width: int = 4056, height: int = 3040):

    byte_width = width * 3 // 2
    stride = byte_width // 32

    if byte_width % 32 != 0:
        stride += 1
        stride *= 32

    rowwise = data.reshape(height, stride)


    # Remove the 28 bytes of padding from each row
    rowwise = rowwise[:, :6084]

    # Flatten back into a contiguous stream
    data = rowwise.ravel()

    b0 = data[0::3].astype(np.uint16)
    b1 = data[1::3].astype(np.uint16)
    b2 = data[2::3].astype(np.uint16)

    p0 = (b0 << 4) | (b2 & 0x0F)
    p1 = (b1 << 4) | (b2 >> 4)

    pixels = np.empty(p0.size + p1.size, dtype=np.uint16)
    pixels[0::2] = p0
    pixels[1::2] = p1

    image = pixels.reshape(height, width)

    return image

if __name__ == "__main__":
    image = load_raw_from_file("image.bin")

    print(np.min(image), np.max(image))

    import matplotlib.pyplot as plt

    plt.imshow(image, cmap="gray", vmin=0, vmax=4095)
    plt.colorbar()
    plt.show()