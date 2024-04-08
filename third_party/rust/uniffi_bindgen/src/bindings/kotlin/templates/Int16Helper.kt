public object FfiConverterShort: FfiConverter<Short, Short> {
    override fun lift(value: Short): Short {
        return value
    }

    override fun read(buf: ByteBuffer): Short {
        return buf.getShort()
    }

    override fun lower(value: Short): Short {
        return value
    }

    override fun allocationSize(value: Short) = 2UL

    override fun write(value: Short, buf: ByteBuffer) {
        buf.putShort(value)
    }
}
