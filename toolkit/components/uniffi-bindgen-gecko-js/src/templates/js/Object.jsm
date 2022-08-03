{%- let object = ci.get_object_definition(name).unwrap() %}

class {{ object.nm() }} {
    // Use `init` to instantiate this class.
    // DO NOT USE THIS CONSTRUCTOR DIRECTLY
    constructor(opts) {
        if (!Object.prototype.hasOwnProperty.call(opts, constructUniffiObject)) {
            throw new UniFFIError("Attempting to construct an object using the JavaScript constructor directly" +
            "Please use a UDL defined constructor, or the init function for the primary constructor")
        }
        if (!opts[constructUniffiObject] instanceof UniFFIPointer) {
            throw new UniFFIError("Attempting to create a UniFFI object with a pointer that is not an instance of UniFFIPointer")
        }
        this[uniffiObjectPtr] = opts[constructUniffiObject];
    }

    {%- for cons in object.constructors() %}
    {%- if cons.is_async() %}
    /**
     * An async constructor for {{ object.nm() }}.
     * 
     * @returns {Promise<{{ object.nm() }}>}: A promise that resolves
     *      to a newly constructed {{ object.nm() }}
     */
    {%- else %}
    /**
     * A constructor for {{ object.nm() }}.
     * 
     * @returns { {{ object.nm() }} }
     */
    {%- endif %}
    static {{ cons.nm() }}({{cons.arg_names()}}) {
        {%- call js::call_constructor(cons, type_) -%}
    }
    {%- endfor %}

    {%- for meth in object.methods() %}
    {{ meth.nm() }}({{ meth.arg_names() }}) {
        {%- call js::call_method(meth, type_, object) -%}
    }
    {%- endfor %}

}

class {{ ffi_converter }} extends FfiConverter {
    static lift(value) {
        const opts = {};
        opts[constructUniffiObject] = value;
        return new {{ object.nm() }}(opts);
    }

    static lower(value) {
        return value[uniffiObjectPtr];
    }

    static read(dataStream) {
        return this.lift(dataStream.readPointer{{ object.nm() }}());
    }

    static write(dataStream, value) {
        dataStream.writePointer{{ object.nm() }}(value[uniffiObjectPtr]);
    }

    static computeSize(value) {
        return 8;
    }
}

EXPORTED_SYMBOLS.push("{{ object.nm() }}");
