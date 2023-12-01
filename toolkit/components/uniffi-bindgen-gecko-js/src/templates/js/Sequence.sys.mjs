// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        const len = dataStream.readInt32();
        const arr = [];
        for (let i = 0; i < len; i++) {
            arr.push({{ inner_type.ffi_converter() }}.read(dataStream));
        }
        return arr;
    }

    static write(dataStream, value) {
        dataStream.writeInt32(value.length);
        value.forEach((innerValue) => {
            {{ inner_type.ffi_converter() }}.write(dataStream, innerValue);
        })
    }

    static computeSize(value) {
        // The size of the length
        let size = 4;
        for (const innerValue of value) {
            size += {{ inner_type.ffi_converter() }}.computeSize(innerValue);
        }
        return size;
    }

    static checkType(value) {
        if (!Array.isArray(value)) {
            throw new UniFFITypeError(`${value} is not an array`);
        }
        value.forEach((innerValue, idx) => {
            try {
                {{ inner_type.ffi_converter() }}.checkType(innerValue);
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart(`[${idx}]`);
                }
                throw e;
            }
        })
    }
}
