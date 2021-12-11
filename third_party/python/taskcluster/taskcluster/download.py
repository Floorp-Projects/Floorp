"""
Support for downloading objects from the object service, following best
practices for that service.

Downloaded data is written to a "writer" provided by a "writer factory".  A
writer has a `write` method which writes the entire passed buffer to storage.
A writer factory is a callable which returns a fresh writer, ready to write the
first byte of the object.  When downloads are retried, the writer factory may
be called more than once.

This module provides several pre-defined writers and writer factories for
common cases.
"""
import functools
import six

if six.PY2:
    raise ImportError("download is only supported in Python 3")

from .aio import download as aio_download
from .aio.asyncutils import ensureCoro, runAsync


def downloadToBuf(**kwargs):
    """
    Convenience method to download data to an in-memory buffer and return the
    downloaded data.  Arguments are the same as `download`, except that
    `writerFactory` should not be supplied.  Returns a tuple (buffer, contentType).
    """
    return runAsync(aio_download.downloadToBuf(**kwargs))


def downloadToFile(file, **kwargs):
    """
    Convenience method to download data to a file object.  The file must be
    writeable, in binary mode, seekable (`f.seek`), and truncatable
    (`f.truncate`) to support retries.  Arguments are the same as `download`,
    except that `writerFactory` should not be supplied.  Returns the content-type.
    """
    return runAsync(aio_download.downloadToFile(file=file, **kwargs))


def download(*, writerFactory, **kwargs):
    """
    Download the named object from the object service, using a writer returned
    from `writerFactory` to write the data.  The `maxRetries` parameter has
    the same meaning as for service clients.  The `objectService` parameter is
    an instance of the Object class, configured with credentials for the
    upload.  Returns the content-type.
    """
    wrappedWriterFactory = _wrapSyncWriterFactory(writerFactory)
    return runAsync(aio_download.download(writerFactory=wrappedWriterFactory, **kwargs))


def downloadArtifactToBuf(**kwargs):
    """
    Convenience method to download an artifact to an in-memory buffer and return the
    downloaded data.  Arguments are the same as `downloadArtifact`, except that
    `writerFactory` should not be supplied.  Returns a tuple (buffer, contentType).
    """
    return runAsync(aio_download.downloadArtifactToBuf(**kwargs))


def downloadArtifactToFile(file, **kwargs):
    """
    Convenience method to download an artifact to a file object.  The file must be
    writeable, in binary mode, seekable (`f.seek`), and truncatable
    (`f.truncate`) to support retries.  Arguments are the same as `downloadArtifac`,
    except that `writerFactory` should not be supplied.  Returns the content-type.
    """
    return runAsync(aio_download.downloadArtifactToFile(file=file, **kwargs))


def downloadArtifact(*, writerFactory, **kwargs):
    """
    Download the named artifact with the appropriate storageType, using a writer returned
    from `writerFactory` to write the data.  The `maxRetries` parameter has
    the same meaning as for service clients.  The `queueService` parameter is
    an instance of the Queue class, configured with credentials for the
    download.  Returns the content-type.
    """
    wrappedWriterFactory = _wrapSyncWriterFactory(writerFactory)
    return runAsync(aio_download.downloadArtifact(writerFactory=wrappedWriterFactory, **kwargs))


def _wrapSyncWriterFactory(writerFactory):
    """Modify the reader returned by readerFactory to have an async read."""
    @functools.wraps(writerFactory)
    async def wrappedFactory():
        writer = writerFactory()
        writer.write = ensureCoro(writer.write)
        return writer

    return wrappedFactory
