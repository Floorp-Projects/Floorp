class {{ ffi_converter }} extends FfiConverter {
    static checkType(name, value) {
        super.checkType(name, value);
        if (!Number.isInteger(value)) {
            throw TypeError(`${name} is not an integer(${value})`);
        }
        if (value < -32768 || value > 32767) {
            throw TypeError(`${name} exceeds the I16 bounds (${value})`);
        }
    }
    static computeSize() {
        return 2;
    }
    static lift(value) {
        return value;
    }
    static lower(value) {
        return value;
    }
    static write(dataStream, value) {
        dataStream.writeInt16(value)
    }
    static read(dataStream) {
        return dataStream.readInt16()
    }
}
