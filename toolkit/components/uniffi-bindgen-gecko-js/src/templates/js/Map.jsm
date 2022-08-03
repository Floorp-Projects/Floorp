class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        const len = dataStream.readInt32();
        const map = {};
        for (let i = 0; i < len; i++) {
            const key = {{ key_type.ffi_converter() }}.read(dataStream);
            const value = {{ value_type.ffi_converter() }}.read(dataStream);
            map[key] = value;
        }

        return map;
    }

    static write(dataStream, value) {
        dataStream.writeInt32(Object.keys(value).length);
        for (const key in value) {
            {{ key_type.ffi_converter() }}.write(dataStream, key);
            {{ value_type.ffi_converter() }}.write(dataStream, value[key]);
        }
    }

    static computeSize(value) {
        // The size of the length
        let size = 4;
        for (const key in value) {
            size += {{ key_type.ffi_converter() }}.computeSize(key);
            size += {{ value_type.ffi_converter() }}.computeSize(value[key]);
        }
        return size;
    }
}

