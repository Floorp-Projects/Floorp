class _UniffiPointerManagerCPython:
    """
    Manage giving out pointers to Python objects on CPython

    This class is used to generate opaque pointers that reference Python objects to pass to Rust.
    It assumes a CPython platform.  See _UniffiPointerManagerGeneral for the alternative.
    """

    def new_pointer(self, obj):
        """
        Get a pointer for an object as a ctypes.c_size_t instance

        Each call to new_pointer() must be balanced with exactly one call to release_pointer()

        This returns a ctypes.c_size_t.  This is always the same size as a pointer and can be
        interchanged with pointers for FFI function arguments and return values.
        """
        # IncRef the object since we're going to pass a pointer to Rust
        ctypes.pythonapi.Py_IncRef(ctypes.py_object(obj))
        # id() is the object address on CPython
        # (https://docs.python.org/3/library/functions.html#id)
        return id(obj)

    def release_pointer(self, address):
        py_obj = ctypes.cast(address, ctypes.py_object)
        obj = py_obj.value
        ctypes.pythonapi.Py_DecRef(py_obj)
        return obj

    def lookup(self, address):
        return ctypes.cast(address, ctypes.py_object).value

class _UniffiPointerManagerGeneral:
    """
    Manage giving out pointers to Python objects on non-CPython platforms

    This has the same API as _UniffiPointerManagerCPython, but doesn't assume we're running on
    CPython and is slightly slower.

    Instead of using real pointers, it maps integer values to objects and returns the keys as
    c_size_t values.
    """

    def __init__(self):
        self._map = {}
        self._lock = threading.Lock()
        self._current_handle = 0

    def new_pointer(self, obj):
        with self._lock:
            handle = self._current_handle
            self._current_handle += 1
            self._map[handle] = obj
        return handle

    def release_pointer(self, handle):
        with self._lock:
            return self._map.pop(handle)

    def lookup(self, handle):
        with self._lock:
            return self._map[handle]

# Pick an pointer manager implementation based on the platform
if platform.python_implementation() == 'CPython':
    _UniffiPointerManager = _UniffiPointerManagerCPython # type: ignore
else:
    _UniffiPointerManager = _UniffiPointerManagerGeneral # type: ignore
