{{ self.add_import("kotlinx.coroutines.CoroutineScope") }}
{{ self.add_import("kotlinx.coroutines.delay") }}
{{ self.add_import("kotlinx.coroutines.isActive") }}
{{ self.add_import("kotlinx.coroutines.launch") }}

internal const val UNIFFI_RUST_TASK_CALLBACK_SUCCESS = 0.toByte()
internal const val UNIFFI_RUST_TASK_CALLBACK_CANCELLED = 1.toByte()
internal const val UNIFFI_FOREIGN_EXECUTOR_CALLBACK_SUCCESS = 0.toByte()
internal const val UNIFFI_FOREIGN_EXECUTOR_CALLBACK_CANCELLED = 1.toByte()
internal const val UNIFFI_FOREIGN_EXECUTOR_CALLBACK_ERROR = 2.toByte()

// Callback function to execute a Rust task.  The Kotlin code schedules these in a coroutine then
// invokes them.
internal interface UniFfiRustTaskCallback : com.sun.jna.Callback {
    fun callback(rustTaskData: Pointer?, statusCode: Byte)
}

internal object UniFfiForeignExecutorCallback : com.sun.jna.Callback {
    fun callback(handle: USize, delayMs: Int, rustTask: UniFfiRustTaskCallback?, rustTaskData: Pointer?) : Byte {
        if (rustTask == null) {
            FfiConverterForeignExecutor.drop(handle)
            return UNIFFI_FOREIGN_EXECUTOR_CALLBACK_SUCCESS
        } else {
            val coroutineScope = FfiConverterForeignExecutor.lift(handle)
            if (coroutineScope.isActive) {
                val job = coroutineScope.launch {
                    if (delayMs > 0) {
                        delay(delayMs.toLong())
                    }
                    rustTask.callback(rustTaskData, UNIFFI_RUST_TASK_CALLBACK_SUCCESS)
                }
                job.invokeOnCompletion { cause ->
                    if (cause != null) {
                        rustTask.callback(rustTaskData, UNIFFI_RUST_TASK_CALLBACK_CANCELLED)
                    }
                }
                return UNIFFI_FOREIGN_EXECUTOR_CALLBACK_SUCCESS
            } else {
                return UNIFFI_FOREIGN_EXECUTOR_CALLBACK_CANCELLED
            }
        }
    }
}

public object FfiConverterForeignExecutor: FfiConverter<CoroutineScope, USize> {
    internal val handleMap = UniFfiHandleMap<CoroutineScope>()

    internal fun drop(handle: USize) {
        handleMap.remove(handle)
    }

    internal fun register(lib: _UniFFILib) {
        {%- match ci.ffi_foreign_executor_callback_set() %}
        {%- when Some with (fn) %}
        lib.{{ fn.name() }}(UniFfiForeignExecutorCallback)
        {%- when None %}
        {#- No foreign executor, we don't set anything #}
        {% endmatch %}
    }

    // Number of live handles, exposed so we can test the memory management
    public fun handleCount() : Int {
        return handleMap.size
    }

    override fun allocationSize(value: CoroutineScope) = USize.size

    override fun lift(value: USize): CoroutineScope {
        return handleMap.get(value) ?: throw RuntimeException("unknown handle in FfiConverterForeignExecutor.lift")
    }

    override fun read(buf: ByteBuffer): CoroutineScope {
        return lift(USize.readFromBuffer(buf))
    }

    override fun lower(value: CoroutineScope): USize {
        return handleMap.insert(value)
    }

    override fun write(value: CoroutineScope, buf: ByteBuffer) {
        lower(value).writeToBuffer(buf)
    }
}
