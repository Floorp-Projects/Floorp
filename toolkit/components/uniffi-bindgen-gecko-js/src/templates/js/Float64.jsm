class {{ ffi_converter }} extends FfiConverter {
    static computeSize() {
        return 8;
    }
    static lift(value) {
        return value;
    }
    static lower(value) {
        return value;
    }
    static write(dataStream, value) {
        dataStream.writeFloat64(value)
    }
    static read(dataStream) {
        return dataStream.readFloat64()
    }
}
