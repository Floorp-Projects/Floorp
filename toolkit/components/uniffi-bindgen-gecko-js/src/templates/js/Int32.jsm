class {{ ffi_converter }} extends FfiConverter {
    static checkType(name, value) {
        super.checkType(name, value);
        if (!Number.isInteger(value)) {
            throw TypeError(`${name} is not an integer(${value})`);
        }
        if (value < -2147483648 || value > 2147483647) {
            throw TypeError(`${name} exceeds the I32 bounds (${value})`);
        }
    }
    static computeSize() {
        return 4;
    }
    static lift(value) {
        return value;
    }
    static lower(value) {
        return value;
    }
    static write(dataStream, value) {
        dataStream.writeInt32(value)
    }
    static read(dataStream) {
        return dataStream.readInt32()
    }
}
