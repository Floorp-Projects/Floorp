{%- let cbi = ci.get_callback_interface_definition(name).unwrap() %}
{#- See CallbackInterfaceRuntime.jsm and CallbackInterfaceHandler.jsm for the callback interface handler definition, referenced here as `{{ cbi.handler() }}` #}
class {{ ffi_converter }} extends FfiConverter {
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
