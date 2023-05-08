// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverter {
    static checkType(value) {
        super.checkType(value);
        if (!Number.isInteger(value)) {
            throw new UniFFITypeError(`${value} is not an integer`);
        }
        if (value < 0 || value > 65535) {
            throw new UniFFITypeError(`${value} exceeds the U16 bounds`);
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
        dataStream.writeUint16(value)
    }
    static read(dataStream) {
        return dataStream.readUint16()
    }
}
