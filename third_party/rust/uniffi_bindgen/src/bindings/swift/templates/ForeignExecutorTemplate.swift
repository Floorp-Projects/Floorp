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

    static func lift(_ value: FfiType) throws -> SwiftType {
        UniFfiForeignExecutor(priority: TaskPriority(rawValue: numericCast(value)))
    }
    static func lower(_ value: SwiftType) -> FfiType {
        numericCast(value.priority.rawValue)
    }

    static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> SwiftType {
        fatalError("FfiConverterForeignExecutor.read not implemented yet")
    }
    static func write(_ value: SwiftType, into buf: inout [UInt8]) {
        fatalError("FfiConverterForeignExecutor.read not implemented yet")
    }
}


fileprivate func uniffiForeignExecutorCallback(executorHandle: Int, delayMs: UInt32, rustTask: UniFfiRustTaskCallback?, taskData: UnsafeRawPointer?) {
    if let rustTask = rustTask {
        let executor = try! FfiConverterForeignExecutor.lift(executorHandle)
        Task.detached(priority: executor.priority) {
            if delayMs != 0 {
                let nanoseconds: UInt64 = numericCast(delayMs * 1000000)
                try! await Task.sleep(nanoseconds: nanoseconds)
            }
            rustTask(taskData)
        }

    }
    // No else branch: when rustTask is null, we should drop the foreign executor. However, since
    // its just a value type, we don't need to do anything here.
}

fileprivate func uniffiInitForeignExecutor() {
    uniffi_foreign_executor_callback_set(uniffiForeignExecutorCallback)
}
