internal typealias Handle = Long
internal class ConcurrentHandleMap<T>(
    private val leftMap: MutableMap<Handle, T> = mutableMapOf(),
    private val rightMap: MutableMap<T, Handle> = mutableMapOf()
) {
    private val lock = java.util.concurrent.locks.ReentrantLock()
    private val currentHandle = AtomicLong(0L)
    private val stride = 1L

    fun insert(obj: T): Handle =
        lock.withLock {
            rightMap[obj] ?:
                currentHandle.getAndAdd(stride)
                    .also { handle ->
                        leftMap[handle] = obj
                        rightMap[obj] = handle
                    }
            }

    fun get(handle: Handle) = lock.withLock {
        leftMap[handle]
    }

    fun delete(handle: Handle) {
        this.remove(handle)
    }

    fun remove(handle: Handle): T? =
        lock.withLock {
            leftMap.remove(handle)?.let { obj ->
                rightMap.remove(obj)
                obj
            }
        }
}

interface ForeignCallback : com.sun.jna.Callback {
    public fun invoke(handle: Handle, method: Int, args: RustBuffer.ByValue, outBuf: RustBufferByReference): Int
}

// Magic number for the Rust proxy to call using the same mechanism as every other method,
// to free the callback once it's dropped by Rust.
internal const val IDX_CALLBACK_FREE = 0

public abstract class FfiConverterCallbackInterface<CallbackInterface>(
    protected val foreignCallback: ForeignCallback
): FfiConverter<CallbackInterface, Handle> {
    private val handleMap = ConcurrentHandleMap<CallbackInterface>()

    // Registers the foreign callback with the Rust side.
    // This method is generated for each callback interface.
    internal abstract fun register(lib: _UniFFILib)

    fun drop(handle: Handle): RustBuffer.ByValue {
        return handleMap.remove(handle).let { RustBuffer.ByValue() }
    }

    override fun lift(value: Handle): CallbackInterface {
        return handleMap.get(value) ?: throw InternalException("No callback in handlemap; this is a Uniffi bug")
    }

    override fun read(buf: ByteBuffer) = lift(buf.getLong())

    override fun lower(value: CallbackInterface) =
        handleMap.insert(value).also {
            assert(handleMap.get(it) === value) { "Handle map is not returning the object we just placed there. This is a bug in the HandleMap." }
        }

    override fun allocationSize(value: CallbackInterface) = 8

    override fun write(value: CallbackInterface, buf: ByteBuffer) {
        buf.putLong(lower(value))
    }
}
