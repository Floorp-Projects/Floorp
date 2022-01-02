"""
Support for downloading objects from the object service, following best
practices for that service.

Downloaded data is written to a "writer" provided by a "writer factory".  A
writer has an async `write` method which writes the entire passed buffer to
storage.  A writer factory is an async callable which returns a fresh writer,
ready to write the first byte of the object.  When downloads are retried, the
writer factory may be called more than once.

Note that `aiofile.open` returns a value suitable for use as a writer, if async
file IO is important to the application.

This module provides several pre-defined writers and writer factories for
common cases.
"""
import six

if six.PY2:
    raise ImportError("download is only supported in Python 3")

import aiohttp
import contextlib

from .asyncutils import ensureCoro
from .reader_writer import streamingCopy, BufferWriter, FileWriter
from .retry import retry
from . import Object
from ..exceptions import TaskclusterArtifactError, TaskclusterFailure


async def downloadToBuf(**kwargs):
    """
    Convenience method to download data to an in-memory buffer and return the
    downloaded data.  Arguments are the same as `download`, except that
    `writerFactory` should not be supplied.  Returns a tuple (buffer, contentType).
    """
    writer = None

    async def writerFactory():
        nonlocal writer
        writer = BufferWriter()
        return writer

    contentType = await download(writerFactory=writerFactory, **kwargs)
    return writer.getbuffer(), contentType


async def downloadToFile(file, **kwargs):
    """
    Convenience method to download data to a file object.  The file must be
    writeable, in binary mode, seekable (`f.seek`), and truncatable
    (`f.truncate`) to support retries.  Arguments are the same as `download`,
    except that `writerFactory` should not be supplied.  Returns the content-type.
    """
    async def writerFactory():
        file.seek(0)
        file.truncate()
        return FileWriter(file)

    return await download(writerFactory=writerFactory, **kwargs)


async def download(*, name, maxRetries=5, objectService, writerFactory):
    """
    Download the named object from the object service, using a writer returned
    from `writerFactory` to write the data.  The `maxRetries` parameter has
    the same meaning as for service clients.  The `objectService` parameter is
    an instance of the Object class, configured with credentials for the
    download.  Returns the content-type.
    """
    async with aiohttp.ClientSession() as session:
        downloadResp = await ensureCoro(objectService.startDownload)(name, {
            "acceptDownloadMethods": {
                "simple": True,
            },
        })

        method = downloadResp["method"]

        if method == "simple":
            async def tryDownload(retryFor):
                with _maybeRetryHttpRequest(retryFor):
                    writer = await writerFactory()
                    url = downloadResp['url']
                    return await _doSimpleDownload(url, writer, session)

            return await retry(maxRetries, tryDownload)
        else:
            raise RuntimeError(f'Unknown download method {method}')


async def downloadArtifactToBuf(**kwargs):
    """
    Convenience method to download an artifact to an in-memory buffer and return the
    downloaded data.  Arguments are the same as `downloadArtifact`, except that
    `writerFactory` should not be supplied.  Returns a tuple (buffer, contentType).
    """
    writer = None

    async def writerFactory():
        nonlocal writer
        writer = BufferWriter()
        return writer

    contentType = await downloadArtifact(writerFactory=writerFactory, **kwargs)
    return writer.getbuffer(), contentType


async def downloadArtifactToFile(file, **kwargs):
    """
    Convenience method to download an artifact to a file object.  The file must be
    writeable, in binary mode, seekable (`f.seek`), and truncatable
    (`f.truncate`) to support retries.  Arguments are the same as `downloadArtifac`,
    except that `writerFactory` should not be supplied.  Returns the content-type.
    """
    async def writerFactory():
        file.seek(0)
        file.truncate()
        return FileWriter(file)

    return await downloadArtifact(writerFactory=writerFactory, **kwargs)


async def downloadArtifact(*, taskId, name, runId=None, maxRetries=5, queueService, writerFactory):
    """
    Download the named artifact with the appropriate storageType, using a writer returned
    from `writerFactory` to write the data.  The `maxRetries` parameter has
    the same meaning as for service clients.  The `queueService` parameter is
    an instance of the Queue class, configured with credentials for the
    download.  Returns the content-type.
    """
    if runId is None:
        artifact = await ensureCoro(queueService.latestArtifact)(taskId, name)
    else:
        artifact = await ensureCoro(queueService.artifact)(taskId, runId, name)

    if artifact["storageType"] == 's3' or artifact["storageType"] == 'reference':
        async with aiohttp.ClientSession() as session:

            async def tryDownload(retryFor):
                with _maybeRetryHttpRequest(retryFor):
                    writer = await writerFactory()
                    return await _doSimpleDownload(artifact["url"], writer, session)

            return await retry(maxRetries, tryDownload)

    elif artifact["storageType"] == 'object':
        objectService = Object({
            "rootUrl": queueService.options["rootUrl"],
            "maxRetries": maxRetries,
            "credentials": artifact["credentials"],
        })
        return await download(
            name=artifact["name"],
            maxRetries=maxRetries,
            objectService=objectService,
            writerFactory=writerFactory)

    elif artifact["storageType"] == 'error':
        raise TaskclusterArtifactError(artifact["message"], artifact["reason"])

    else:
        raise TaskclusterFailure(f"Unknown storageType f{artifact['storageType']}")


@contextlib.contextmanager
def _maybeRetryHttpRequest(retryFor):
    "Catch errors from an aiohttp request and retry the retriable responses."
    try:
        yield
    except aiohttp.ClientResponseError as exc:
        # treat 4xx's as fatal, and retry others
        if 400 <= exc.status < 500:
            raise exc
        return retryFor(exc)
    except aiohttp.ClientError as exc:
        # retry for all other aiohttp errors
        return retryFor(exc)
    # .. anything else is considered fatal


async def _doSimpleDownload(url, writer, session):
    async with session.get(url) as resp:
        contentType = resp.content_type
        resp.raise_for_status()
        # note that `resp.content` is a StreamReader and satisfies the
        # requirements of a reader in this case
        await streamingCopy(resp.content, writer)

    return contentType
