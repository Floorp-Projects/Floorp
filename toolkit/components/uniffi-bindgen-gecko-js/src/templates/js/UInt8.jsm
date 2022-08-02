class {{ ffi_converter }} extends FfiConverter {
    static checkType(name, value) {
        super.checkType(name, value);
        if (!Number.isInteger(value)) {
            throw TypeError(`${name} is not an integer(${value})`);
        }
        if (value < 0 || value > 256) {
            throw TypeError(`${name} exceeds the U8 bounds (${value})`);
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
        dataStream.writeUint8(value)
    }
    static read(dataStream) {
        return dataStream.readUint8()
    }
}
