{%- let cbi = ci.get_callback_interface_definition(name).unwrap() %}
{#- See CallbackInterfaceRuntime.sys.mjs and CallbackInterfaceHandler.sys.mjs for the callback interface handler definition, referenced here as `{{ cbi.handler() }}` #}
// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverter {
    static lower(callbackObj) {
        return {{ cbi.handler() }}.storeCallbackObj(callbackObj)
    }

    static lift(handleId) {
        return {{ cbi.handler() }}.getCallbackObj(handleId)
    }

    static read(dataStream) {
        return this.lift(dataStream.readInt64())
    }

    static write(dataStream, callbackObj) {
        dataStream.writeInt64(this.lower(callbackObj))
    }

    static computeSize(callbackObj) {
        return 8;
    }
}
