"""
Utilities supporting the "reader" and "writer" definitions used in uploads and downloads.
"""
import asyncio
import io


class BufferWriter:
    """A writer that writes to an in-memory buffer"""
    def __init__(self):
        self.buf = io.BytesIO()

    async def write(self, chunk):
        self.buf.write(chunk)

    def getbuffer(self):
        """Get the content of the in-memory buffer"""
        return self.buf.getbuffer()


class BufferReader:
    """A reader that reads from an in-memory buffer"""
    def __init__(self, data):
        self.buf = io.BytesIO(data)

    async def read(self, max_size):
        return self.buf.read(max_size)


class FileWriter:
    """A writer that writes to a (sync) file.  The file should be opened in binary mode
    and empty."""
    def __init__(self, file):
        self.file = file

    async def write(self, chunk):
        self.file.write(chunk)


class FileReader:
    """A reader that reads from a (sync) file.  The file should be opened in binary mode,
    and positioned at its beginning."""
    def __init__(self, file):
        self.file = file

    async def read(self, max_size):
        return self.file.read(max_size)


async def streamingCopy(reader, writer):
    "Copy data from a reader to a writer, as those are defined in upload.py and download.py"
    # we will read and write concurrently, but with limited buffering -- just enough
    # that read and write operations are not forced to alternate
    chunk_size = 64 * 1024
    q = asyncio.Queue(maxsize=1)

    async def read_loop():
        while True:
            chunk = await reader.read(chunk_size)
            await q.put(chunk)
            if not chunk:
                break

    async def write_loop():
        while True:
            chunk = await q.get()
            if not chunk:
                q.task_done()
                break
            await writer.write(chunk)
            q.task_done()

    read_task = asyncio.ensure_future(read_loop())
    write_task = asyncio.ensure_future(write_loop())

    try:
        await asyncio.gather(read_task, write_task)
    finally:
        # cancel any still-running tasks, as in case of an exception
        read_task.cancel()
        write_task.cancel()
