{%- let obj = ci|get_object_definition(name) %}
{%- if self.include_once_check("ObjectRuntime.kt") %}{% include "ObjectRuntime.kt" %}{% endif %}
{{- self.add_import("java.util.concurrent.atomic.AtomicLong") }}
{{- self.add_import("java.util.concurrent.atomic.AtomicBoolean") }}

public interface {{ type_name }}Interface {
    {% for meth in obj.methods() -%}
    {%- match meth.throws_type() -%}
    {%- when Some with (throwable) -%}
    @Throws({{ throwable|error_type_name }}::class)
    {%- when None -%}
    {%- endmatch %}
    {% if meth.is_async() -%}
    suspend fun {{ meth.name()|fn_name }}({% call kt::arg_list_decl(meth) %})
    {%- else -%}
    fun {{ meth.name()|fn_name }}({% call kt::arg_list_decl(meth) %})
    {%- endif %}
    {%- match meth.return_type() -%}
    {%- when Some with (return_type) %}: {{ return_type|type_name -}}
    {%- when None -%}
    {%- endmatch -%}

    {% endfor %}
}

class {{ type_name }}(
    pointer: Pointer
) : FFIObject(pointer), {{ type_name }}Interface {

    {%- match obj.primary_constructor() %}
    {%- when Some with (cons) %}
    constructor({% call kt::arg_list_decl(cons) -%}) :
        this({% call kt::to_ffi_call(cons) %})
    {%- when None %}
    {%- endmatch %}

    /**
     * Disconnect the object from the underlying Rust object.
     *
     * It can be called more than once, but once called, interacting with the object
     * causes an `IllegalStateException`.
     *
     * Clients **must** call this method once done with the object, or cause a memory leak.
     */
    override protected fun freeRustArcPtr() {
        rustCall() { status ->
            _UniFFILib.INSTANCE.{{ obj.ffi_object_free().name() }}(this.pointer, status)
        }
    }

    {% for meth in obj.methods() -%}
    {%- match meth.throws_type() -%}
    {%- when Some with (throwable) %}
    @Throws({{ throwable|error_type_name }}::class)
    {%- else -%}
    {%- endmatch -%}
    {%- if meth.is_async() %}
    @Suppress("ASSIGNED_BUT_NEVER_ACCESSED_VARIABLE")
    override suspend fun {{ meth.name()|fn_name }}({%- call kt::arg_list_decl(meth) -%}){% match meth.return_type() %}{% when Some with (return_type) %} : {{ return_type|type_name }}{% when None %}{%- endmatch %} {
        // Create a new `CoroutineScope` for this operation, suspend the coroutine, and call the
        // scaffolding function, passing it one of the callback handlers from `AsyncTypes.kt`.
        //
        // Make sure to retain a reference to the callback handler to ensure that it's not GCed before
        // it's invoked
        var callbackHolder: {{ func.result_type().borrow()|future_callback_handler }}? = null
        return coroutineScope {
            val scope = this
            return@coroutineScope suspendCoroutine { continuation ->
                try {
                    val callback = {{ meth.result_type().borrow()|future_callback_handler }}(continuation)
                    callbackHolder = callback
                    callWithPointer { thisPtr ->
                        rustCall { status ->
                            _UniFFILib.INSTANCE.{{ meth.ffi_func().name() }}(
                                thisPtr,
                                {% call kt::arg_list_lowered(meth) %}
                                FfiConverterForeignExecutor.lower(scope),
                                callback,
                                USize(0),
                                status,
                            )
                        }
                    }
                } catch (e: Exception) {
                    continuation.resumeWithException(e)
                }
            }
        }
    }
    {%- else -%}
    {%- match meth.return_type() -%}
    {%- when Some with (return_type) -%}
    override fun {{ meth.name()|fn_name }}({% call kt::arg_list_protocol(meth) %}): {{ return_type|type_name }} =
        callWithPointer {
            {%- call kt::to_ffi_call_with_prefix("it", meth) %}
        }.let {
            {{ return_type|lift_fn }}(it)
        }

    {%- when None -%}
    override fun {{ meth.name()|fn_name }}({% call kt::arg_list_protocol(meth) %}) =
        callWithPointer {
            {%- call kt::to_ffi_call_with_prefix("it", meth) %}
        }
    {% endmatch %}
    {% endif %}
    {% endfor %}

    {% if !obj.alternate_constructors().is_empty() -%}
    companion object {
        {% for cons in obj.alternate_constructors() -%}
        fun {{ cons.name()|fn_name }}({% call kt::arg_list_decl(cons) %}): {{ type_name }} =
            {{ type_name }}({% call kt::to_ffi_call(cons) %})
        {% endfor %}
    }
    {% endif %}
}

public object {{ obj|ffi_converter_name }}: FfiConverter<{{ type_name }}, Pointer> {
    override fun lower(value: {{ type_name }}): Pointer = value.callWithPointer { it }

    override fun lift(value: Pointer): {{ type_name }} {
        return {{ type_name }}(value)
    }

    override fun read(buf: ByteBuffer): {{ type_name }} {
        // The Rust code always writes pointers as 8 bytes, and will
        // fail to compile if they don't fit.
        return lift(Pointer(buf.getLong()))
    }

    override fun allocationSize(value: {{ type_name }}) = 8

    override fun write(value: {{ type_name }}, buf: ByteBuffer) {
        // The Rust code always expects pointers written as 8 bytes,
        // and will fail to compile if they don't fit.
        buf.putLong(Pointer.nativeValue(lower(value)))
    }
}
