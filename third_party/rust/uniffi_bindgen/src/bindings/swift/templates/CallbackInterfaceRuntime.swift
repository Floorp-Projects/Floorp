fileprivate extension NSLock {
    func withLock<T>(f: () throws -> T) rethrows -> T {
        self.lock()
        defer { self.unlock() }
        return try f()
    }
}

fileprivate typealias UniFFICallbackHandle = UInt64
fileprivate class UniFFICallbackHandleMap<T> {
    private var leftMap: [UniFFICallbackHandle: T] = [:]
    private var counter: [UniFFICallbackHandle: UInt64] = [:]
    private var rightMap: [ObjectIdentifier: UniFFICallbackHandle] = [:]

    private let lock = NSLock()
    private var currentHandle: UniFFICallbackHandle = 0
    private let stride: UniFFICallbackHandle = 1

    func insert(obj: T) -> UniFFICallbackHandle {
        lock.withLock {
            let id = ObjectIdentifier(obj as AnyObject)
            let handle = rightMap[id] ?? {
                currentHandle += stride
                let handle = currentHandle
                leftMap[handle] = obj
                rightMap[id] = handle
                return handle
            }()
            counter[handle] = (counter[handle] ?? 0) + 1
            return handle
        }
    }

    func get(handle: UniFFICallbackHandle) -> T? {
        lock.withLock {
            leftMap[handle]
        }
    }

    func delete(handle: UniFFICallbackHandle) {
        remove(handle: handle)
    }

    @discardableResult
    func remove(handle: UniFFICallbackHandle) -> T? {
        lock.withLock {
            defer { counter[handle] = (counter[handle] ?? 1) - 1 }
            guard counter[handle] == 1 else { return leftMap[handle] }
            let obj = leftMap.removeValue(forKey: handle)
            if let obj = obj {
                rightMap.removeValue(forKey: ObjectIdentifier(obj as AnyObject))
            }
            return obj
        }
    }
}

// Magic number for the Rust proxy to call using the same mechanism as every other method,
// to free the callback once it's dropped by Rust.
private let IDX_CALLBACK_FREE: Int32 = 0
