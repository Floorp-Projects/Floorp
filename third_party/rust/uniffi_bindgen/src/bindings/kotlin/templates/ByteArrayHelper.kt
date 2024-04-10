public object FfiConverterByteArray: FfiConverterRustBuffer<ByteArray> {
    override fun read(buf: ByteBuffer): ByteArray {
        val len = buf.getInt()
        val byteArr = ByteArray(len)
        buf.get(byteArr)
        return byteArr
    }
    override fun allocationSize(value: ByteArray): ULong {
        return 4UL + value.size.toULong()
    }
    override fun write(value: ByteArray, buf: ByteBuffer) {
        buf.putInt(value.size)
        buf.put(value)
    }
}
