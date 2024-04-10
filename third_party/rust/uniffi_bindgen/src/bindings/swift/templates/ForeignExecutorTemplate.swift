private let UNIFFI_RUST_TASK_CALLBACK_SUCCESS: Int8 = 0
private let UNIFFI_RUST_TASK_CALLBACK_CANCELLED: Int8 = 1
private let UNIFFI_FOREIGN_EXECUTOR_CALLBACK_SUCCESS: Int8 = 0
private let UNIFFI_FOREIGN_EXECUTOR_CALLBACK_CANCELED: Int8 = 1
private let UNIFFI_FOREIGN_EXECUTOR_CALLBACK_ERROR: Int8 = 2

// Encapsulates an executor that can run Rust tasks
//
// On Swift, `Task.detached` can handle this we just need to know what priority to send it.
public struct UniFfiForeignExecutor {
    var priority: TaskPriority

    public init(priority: TaskPriority) {
        self.priority = priority
    }

    public init() {
        self.priority = Task.currentPriority
    }
}

fileprivate struct FfiConverterForeignExecutor: FfiConverter {
    typealias SwiftType = UniFfiForeignExecutor
    // Rust uses a pointer to represent the FfiConverterForeignExecutor, but we only need a u8.
    // let's use `Int`, which is equivalent to `size_t`
    typealias FfiType = Int

    public static func lift(_ value: FfiType) throws -> SwiftType {
        UniFfiForeignExecutor(priority: TaskPriority(rawValue: numericCast(value)))
    }
    public static func lower(_ value: SwiftType) -> FfiType {
        numericCast(value.priority.rawValue)
    }

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> SwiftType {
        fatalError("FfiConverterForeignExecutor.read not implemented yet")
    }
    public static func write(_ value: SwiftType, into buf: inout [UInt8]) {
        fatalError("FfiConverterForeignExecutor.read not implemented yet")
    }
}


fileprivate func uniffiForeignExecutorCallback(executorHandle: Int, delayMs: UInt32, rustTask: UniFfiRustTaskCallback?, taskData: UnsafeRawPointer?) -> Int8 {
    if let rustTask = rustTask {
        let executor = try! FfiConverterForeignExecutor.lift(executorHandle)
        Task.detached(priority: executor.priority) {
            if delayMs != 0 {
                let nanoseconds: UInt64 = numericCast(delayMs * 1000000)
                try! await Task.sleep(nanoseconds: nanoseconds)
            }
            rustTask(taskData, UNIFFI_RUST_TASK_CALLBACK_SUCCESS)
        }
        return UNIFFI_FOREIGN_EXECUTOR_CALLBACK_SUCCESS
    } else {
        // When rustTask is null, we should drop the foreign executor.
        // However, since its just a value type, we don't need to do anything here.
        return UNIFFI_FOREIGN_EXECUTOR_CALLBACK_SUCCESS
    }
}

fileprivate func uniffiInitForeignExecutor() {
    {%- match ci.ffi_foreign_executor_callback_set() %}
    {%- when Some with (fn) %}
    {{ fn.name() }}(uniffiForeignExecutorCallback)
    {%- when None %}
    {#- No foreign executor, we don't set anything #}
    {% endmatch %}
}
