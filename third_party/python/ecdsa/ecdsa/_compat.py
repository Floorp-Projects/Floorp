"""
Common functions for providing cross-python version compatibility.
"""
import sys
from six import integer_types


def str_idx_as_int(string, index):
    """Take index'th byte from string, return as integer"""
    val = string[index]
    if isinstance(val, integer_types):
        return val
    return ord(val)


if sys.version_info < (3, 0):
    def normalise_bytes(buffer_object):
        """Cast the input into array of bytes."""
        # flake8 runs on py3 where `buffer` indeed doesn't exist...
        return buffer(buffer_object)  # noqa: F821

    def hmac_compat(ret):
        return ret

else:
    if sys.version_info < (3, 4):
        # on python 3.3 hmac.hmac.update() accepts only bytes, on newer
        # versions it does accept memoryview() also
        def hmac_compat(data):
            if not isinstance(data, bytes):
                return bytes(data)
            return data
    else:
        def hmac_compat(data):
            return data

    def normalise_bytes(buffer_object):
        """Cast the input into array of bytes."""
        return memoryview(buffer_object).cast('B')
