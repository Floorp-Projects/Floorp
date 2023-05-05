// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverter {
    static checkType(value) {
        super.checkType(value);
        if (!Number.isSafeInteger(value)) {
            throw new UniFFITypeError(`${value} exceeds the safe integer bounds`);
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
