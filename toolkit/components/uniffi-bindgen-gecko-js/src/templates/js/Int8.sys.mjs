// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverter {
    static checkType(value) {
        super.checkType(value);
        if (!Number.isInteger(value)) {
            throw new UniFFITypeError(`${value} is not an integer`);
        }
        if (value < -128 || value > 127) {
            throw new UniFFITypeError(`${value} exceeds the I8 bounds`);
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
