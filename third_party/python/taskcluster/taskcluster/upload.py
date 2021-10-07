"""
Support for uploading objects to the object service, following best
practices for that service.

Data for upload is read from a "reader" provided by a "reader factory".  A
reader has a `read(max_size)` method which reads and returns a chunk of 1 ..
`max_size` bytes, or returns an empty string at EOF.  A reader factory is a
callable which returns a fresh reader, ready to read the first byte of the
object.  When uploads are retried, the reader factory may be called more than
once.

This module provides several pre-defined readers and reader factories for
common cases.
"""
import functools
import six

if six.PY2:
    raise ImportError("upload is only supported in Python 3")

from .aio import upload as aio_upload
from .aio.asyncutils import ensureCoro, runAsync


DATA_INLINE_MAX_SIZE = 8192


def uploadFromBuf(*, data, **kwargs):
    """
    Convenience method to upload data from an in-memory buffer.  Arguments are the same
    as `upload` except that `readerFactory` should not be supplied.
    """
    return runAsync(aio_upload.uploadFromBuf(data=data, **kwargs))


def uploadFromFile(*, file, **kwargs):
    """
    Convenience method to upload data from a a file.  The file should be open
    for reading, in binary mode, and be seekable (`f.seek`).  Remaining
    arguments are the same as `upload` except that `readerFactory` should not
    be supplied.
    """
    return runAsync(aio_upload.uploadFromFile(file=file, **kwargs))


def upload(*, readerFactory, **kwargs):
    """
    Upload the given data to the object service with the given metadata.
    The `maxRetries` parameter has the same meaning as for service clients.
    The `objectService` parameter is an instance of the Object class,
    configured with credentials for the upload.
    """
    wrappedReaderFactory = _wrapSyncReaderFactory(readerFactory)
    return runAsync(aio_upload.upload(readerFactory=wrappedReaderFactory, **kwargs))


def _wrapSyncReaderFactory(readerFactory):
    """Modify the reader returned by readerFactory to have an async read."""
    @functools.wraps(readerFactory)
    async def wrappedFactory():
        reader = readerFactory()
        reader.read = ensureCoro(reader.read)
        return reader

    return wrappedFactory
