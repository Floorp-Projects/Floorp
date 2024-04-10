public object FfiConverterFloat: FfiConverter<Float, Float> {
    override fun lift(value: Float): Float {
        return value
    }

    override fun read(buf: ByteBuffer): Float {
        return buf.getFloat()
    }

    override fun lower(value: Float): Float {
        return value
    }

    override fun allocationSize(value: Float) = 4

    override fun write(value: Float, buf: ByteBuffer) {
        buf.putFloat(value)
    }
}
