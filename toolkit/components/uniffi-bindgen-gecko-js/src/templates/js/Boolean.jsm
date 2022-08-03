class {{ ffi_converter }} extends FfiConverter {
    static computeSize() {
        return 1;
    }
    static lift(value) {
        return value == 1;
    }
    static lower(value) {
        if (value) {
            return 1;
        } else {
            return 0;
        }
    }
    static write(dataStream, value) {
        dataStream.writeUint8(this.lower(value))
    }
    static read(dataStream) {
        return this.lift(dataStream.readUint8())
    }
}
