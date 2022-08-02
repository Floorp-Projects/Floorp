class {{ ffi_converter }} extends FfiConverter {
    static checkType(name, value) {
        super.checkType(name, value);
        if (!Number.isSafeInteger(value)) {
            throw TypeError(`${name} exceeds the safe integer bounds (${value})`);
        }
    }
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
        dataStream.writeInt64(value)
    }
    static read(dataStream) {
        return dataStream.readInt64()
    }
}
