class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static checkType(name, value) {
        if (value !== undefined && value !== null) {
            {{ inner.ffi_converter() }}.checkType(name, value)
        }
    }

    static read(dataStream) {
        const code = dataStream.readUint8(0);
        switch (code) {
            case 0:
                return null
            case 1:
                return {{ inner.ffi_converter() }}.read(dataStream)
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
        {{ inner.ffi_converter() }}.write(dataStream, value)
    }

    static computeSize(value) {
        if (value === null || value === undefined) {
            return 1;
        }
        return 1 + {{ inner.ffi_converter() }}.computeSize(value)
    }
}

