import threading

class ConcurrentHandleMap:
    """
    A map where inserting, getting and removing data is synchronized with a lock.
    """

    def __init__(self):
        # type Handle = int
        self._left_map = {}  # type: Dict[Handle, Any]
        self._right_map = {}  # type: Dict[Any, Handle]

        self._lock = threading.Lock()
        self._current_handle = 0
        self._stride = 1


    def insert(self, obj):
        with self._lock:
            if obj in self._right_map:
                return self._right_map[obj]
            else:
                handle = self._current_handle
                self._current_handle += self._stride
                self._left_map[handle] = obj
                self._right_map[obj] = handle
                return handle

    def get(self, handle):
        with self._lock:
            return self._left_map.get(handle)

    def remove(self, handle):
        with self._lock:
            if handle in self._left_map:
                obj = self._left_map.pop(handle)
                del self._right_map[obj]
                return obj

# Magic number for the Rust proxy to call using the same mechanism as every other method,
# to free the callback once it's dropped by Rust.
IDX_CALLBACK_FREE = 0
# Return codes for callback calls
UNIFFI_CALLBACK_SUCCESS = 0
UNIFFI_CALLBACK_ERROR = 1
UNIFFI_CALLBACK_UNEXPECTED_ERROR = 2

class FfiConverterCallbackInterface:
    _handle_map = ConcurrentHandleMap()

    def __init__(self, cb):
        self._foreign_callback = cb

    def drop(self, handle):
        self.__class__._handle_map.remove(handle)

    @classmethod
    def lift(cls, handle):
        obj = cls._handle_map.get(handle)
        if not obj:
            raise InternalError("The object in the handle map has been dropped already")

        return obj

    @classmethod
    def read(cls, buf):
        handle = buf.readU64()
        cls.lift(handle)

    @classmethod
    def lower(cls, cb):
        handle = cls._handle_map.insert(cb)
        return handle

    @classmethod
    def write(cls, cb, buf):
        buf.writeU64(cls.lower(cb))
