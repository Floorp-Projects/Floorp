
class RustBuffer(ctypes.Structure):
    _fields_ = [
        ("capacity", ctypes.c_int32),
        ("len", ctypes.c_int32),
        ("data", ctypes.POINTER(ctypes.c_char)),
    ]

    @staticmethod
    def alloc(size):
        return rust_call(_UniFFILib.{{ ci.ffi_rustbuffer_alloc().name() }}, size)

    @staticmethod
    def reserve(rbuf, additional):
        return rust_call(_UniFFILib.{{ ci.ffi_rustbuffer_reserve().name() }}, rbuf, additional)

    def free(self):
        return rust_call(_UniFFILib.{{ ci.ffi_rustbuffer_free().name() }}, self)

    def __str__(self):
        return "RustBuffer(capacity={}, len={}, data={})".format(
            self.capacity,
            self.len,
            self.data[0:self.len]
        )

    @contextlib.contextmanager
    def allocWithBuilder(*args):
        """Context-manger to allocate a buffer using a RustBufferBuilder.

        The allocated buffer will be automatically freed if an error occurs, ensuring that
        we don't accidentally leak it.
        """
        builder = RustBufferBuilder()
        try:
            yield builder
        except:
            builder.discard()
            raise

    @contextlib.contextmanager
    def consumeWithStream(self):
        """Context-manager to consume a buffer using a RustBufferStream.

        The RustBuffer will be freed once the context-manager exits, ensuring that we don't
        leak it even if an error occurs.
        """
        try:
            s = RustBufferStream.from_rust_buffer(self)
            yield s
            if s.remaining() != 0:
                raise RuntimeError("junk data left in buffer at end of consumeWithStream")
        finally:
            self.free()

    @contextlib.contextmanager
    def readWithStream(self):
        """Context-manager to read a buffer using a RustBufferStream.

        This is like consumeWithStream, but doesn't free the buffer afterwards.
        It should only be used with borrowed `RustBuffer` data.
        """
        s = RustBufferStream.from_rust_buffer(self)
        yield s
        if s.remaining() != 0:
            raise RuntimeError("junk data left in buffer at end of readWithStream")

class ForeignBytes(ctypes.Structure):
    _fields_ = [
        ("len", ctypes.c_int32),
        ("data", ctypes.POINTER(ctypes.c_char)),
    ]

    def __str__(self):
        return "ForeignBytes(len={}, data={})".format(self.len, self.data[0:self.len])


class RustBufferStream:
    """
    Helper for structured reading of bytes from a RustBuffer
    """

    def __init__(self, data, len):
        self.data = data
        self.len = len
        self.offset = 0

    @classmethod
    def from_rust_buffer(cls, buf):
        return cls(buf.data, buf.len)

    def remaining(self):
        return self.len - self.offset

    def _unpack_from(self, size, format):
        if self.offset + size > self.len:
            raise InternalError("read past end of rust buffer")
        value = struct.unpack(format, self.data[self.offset:self.offset+size])[0]
        self.offset += size
        return value

    def read(self, size):
        if self.offset + size > self.len:
            raise InternalError("read past end of rust buffer")
        data = self.data[self.offset:self.offset+size]
        self.offset += size
        return data

    def readI8(self):
        return self._unpack_from(1, ">b")

    def readU8(self):
        return self._unpack_from(1, ">B")

    def readI16(self):
        return self._unpack_from(2, ">h")

    def readU16(self):
        return self._unpack_from(2, ">H")

    def readI32(self):
        return self._unpack_from(4, ">i")

    def readU32(self):
        return self._unpack_from(4, ">I")

    def readI64(self):
        return self._unpack_from(8, ">q")

    def readU64(self):
        return self._unpack_from(8, ">Q")

    def readFloat(self):
        v = self._unpack_from(4, ">f")
        return v

    def readDouble(self):
        return self._unpack_from(8, ">d")

    def readCSizeT(self):
        return self._unpack_from(ctypes.sizeof(ctypes.c_size_t) , "@N")

class RustBufferBuilder:
    """
    Helper for structured writing of bytes into a RustBuffer.
    """

    def __init__(self):
        self.rbuf = RustBuffer.alloc(16)
        self.rbuf.len = 0

    def finalize(self):
        rbuf = self.rbuf
        self.rbuf = None
        return rbuf

    def discard(self):
        if self.rbuf is not None:
            rbuf = self.finalize()
            rbuf.free()

    @contextlib.contextmanager
    def _reserve(self, numBytes):
        if self.rbuf.len + numBytes > self.rbuf.capacity:
            self.rbuf = RustBuffer.reserve(self.rbuf, numBytes)
        yield None
        self.rbuf.len += numBytes

    def _pack_into(self, size, format, value):
        with self._reserve(size):
            # XXX TODO: I feel like I should be able to use `struct.pack_into` here but can't figure it out.
            for i, byte in enumerate(struct.pack(format, value)):
                self.rbuf.data[self.rbuf.len + i] = byte

    def write(self, value):
        with self._reserve(len(value)):
            for i, byte in enumerate(value):
                self.rbuf.data[self.rbuf.len + i] = byte

    def writeI8(self, v):
        self._pack_into(1, ">b", v)

    def writeU8(self, v):
        self._pack_into(1, ">B", v)

    def writeI16(self, v):
        self._pack_into(2, ">h", v)

    def writeU16(self, v):
        self._pack_into(2, ">H", v)

    def writeI32(self, v):
        self._pack_into(4, ">i", v)

    def writeU32(self, v):
        self._pack_into(4, ">I", v)

    def writeI64(self, v):
        self._pack_into(8, ">q", v)

    def writeU64(self, v):
        self._pack_into(8, ">Q", v)

    def writeFloat(self, v):
        self._pack_into(4, ">f", v)

    def writeDouble(self, v):
        self._pack_into(8, ">d", v)

    def writeCSizeT(self, v):
        self._pack_into(ctypes.sizeof(ctypes.c_size_t) , "@N", v)
