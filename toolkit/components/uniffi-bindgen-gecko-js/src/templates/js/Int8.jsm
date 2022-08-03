class {{ ffi_converter }} extends FfiConverter {
    static checkType(name, value) {
        super.checkType(name, value);
        if (!Number.isInteger(value)) {
            throw TypeError(`${name} is not an integer(${value})`);
        }
        if (value < -128 || value > 127) {
            throw TypeError(`${name} exceeds the I8 bounds (${value})`);
        }
    }
    static computeSize() {
        return 1;
    }
    static lift(value) {
        return value;
    }
    static lower(value) {
        return value;
    }
    static write(dataStream, value) {
        dataStream.writeInt8(value)
    }
    static read(dataStream) {
        return dataStream.readInt8()
    }
}
