public object FfiConverterByte: FfiConverter<Byte, Byte> {
    override fun lift(value: Byte): Byte {
        return value
    }

    override fun read(buf: ByteBuffer): Byte {
        return buf.get()
    }

    override fun lower(value: Byte): Byte {
        return value
    }

    override fun allocationSize(value: Byte) = 1UL

    override fun write(value: Byte, buf: ByteBuffer) {
        buf.put(value)
    }
}
