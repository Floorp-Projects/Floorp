// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static checkType(value) {
        if (value !== undefined && value !== null) {
            {{ inner_type.ffi_converter() }}.checkType(value)
        }
    }

    static read(dataStream) {
        const code = dataStream.readUint8(0);
        switch (code) {
            case 0:
                return null
            case 1:
                return {{ inner_type.ffi_converter() }}.read(dataStream)
            default:
                throw UniFFIError(`Unexpected code: ${code}`);
        }
    }

    static write(dataStream, value) {
        if (value === null || value === undefined) {
            dataStream.writeUint8(0);
            return;
        }
        dataStream.writeUint8(1);
        {{ inner_type.ffi_converter() }}.write(dataStream, value)
    }

    static computeSize(value) {
        if (value === null || value === undefined) {
            return 1;
        }
        return 1 + {{ inner_type.ffi_converter() }}.computeSize(value)
    }
}
