// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverter {
    static checkType(value) {
        super.checkType(value);
        if (typeof value !== "string") {
            throw new UniFFITypeError(`${value} is not a string`);
        }
    }

    static lift(buf) {
        const decoder = new TextDecoder();
        const utf8Arr = new Uint8Array(buf);
        return decoder.decode(utf8Arr);
    }
    static lower(value) {
        const encoder = new TextEncoder();
        return encoder.encode(value).buffer;
    }

    static write(dataStream, value) {
        dataStream.writeString(value);
    }

    static read(dataStream) {
        return dataStream.readString();
    }

    static computeSize(value) {
        const encoder = new TextEncoder();
        return 4 + encoder.encode(value).length
    }
}
