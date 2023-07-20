{{ self.add_import("kotlinx.coroutines.CoroutineScope") }}
{{ self.add_import("kotlinx.coroutines.delay") }}
{{ self.add_import("kotlinx.coroutines.launch") }}

// Callback function to execute a Rust task.  The Kotlin code schedules these in a coroutine then
// invokes them.
internal interface UniFfiRustTaskCallback : com.sun.jna.Callback {
    fun invoke(rustTaskData: Pointer?)
}

object UniFfiForeignExecutorCallback : com.sun.jna.Callback {
    internal fun invoke(handle: USize, delayMs: Int, rustTask: UniFfiRustTaskCallback?, rustTaskData: Pointer?) {
        if (rustTask == null) {
            FfiConverterForeignExecutor.drop(handle)
        } else {
            val coroutineScope = FfiConverterForeignExecutor.lift(handle)
            coroutineScope.launch {
                if (delayMs > 0) {
                    delay(delayMs.toLong())
                }
                rustTask.invoke(rustTaskData)
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
        lib.uniffi_foreign_executor_callback_set(UniFfiForeignExecutorCallback)
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
