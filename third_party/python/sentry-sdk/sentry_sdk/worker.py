import os

from threading import Thread, Lock
from time import sleep, time
from sentry_sdk._compat import queue, check_thread_support
from sentry_sdk.utils import logger

from sentry_sdk._types import MYPY

if MYPY:
    from queue import Queue
    from typing import Any
    from typing import Optional
    from typing import Callable


_TERMINATOR = object()


class BackgroundWorker(object):
    def __init__(self):
        # type: () -> None
        check_thread_support()
        self._queue = queue.Queue(30)  # type: Queue[Any]
        self._lock = Lock()
        self._thread = None  # type: Optional[Thread]
        self._thread_for_pid = None  # type: Optional[int]

    @property
    def is_alive(self):
        # type: () -> bool
        if self._thread_for_pid != os.getpid():
            return False
        if not self._thread:
            return False
        return self._thread.is_alive()

    def _ensure_thread(self):
        # type: () -> None
        if not self.is_alive:
            self.start()

    def _timed_queue_join(self, timeout):
        # type: (float) -> bool
        deadline = time() + timeout
        queue = self._queue

        real_all_tasks_done = getattr(
            queue, "all_tasks_done", None
        )  # type: Optional[Any]
        if real_all_tasks_done is not None:
            real_all_tasks_done.acquire()
            all_tasks_done = real_all_tasks_done  # type: Optional[Any]
        elif queue.__module__.startswith("eventlet."):
            all_tasks_done = getattr(queue, "_cond", None)
        else:
            all_tasks_done = None

        try:
            while queue.unfinished_tasks:
                delay = deadline - time()
                if delay <= 0:
                    return False
                if all_tasks_done is not None:
                    all_tasks_done.wait(timeout=delay)
                else:
                    # worst case, we just poll the number of remaining tasks
                    sleep(0.1)

            return True
        finally:
            if real_all_tasks_done is not None:
                real_all_tasks_done.release()

    def start(self):
        # type: () -> None
        with self._lock:
            if not self.is_alive:
                self._thread = Thread(
                    target=self._target, name="raven-sentry.BackgroundWorker"
                )
                self._thread.setDaemon(True)
                self._thread.start()
                self._thread_for_pid = os.getpid()

    def kill(self):
        # type: () -> None
        """
        Kill worker thread. Returns immediately. Not useful for
        waiting on shutdown for events, use `flush` for that.
        """
        logger.debug("background worker got kill request")
        with self._lock:
            if self._thread:
                try:
                    self._queue.put_nowait(_TERMINATOR)
                except queue.Full:
                    logger.debug("background worker queue full, kill failed")

                self._thread = None
                self._thread_for_pid = None

    def flush(self, timeout, callback=None):
        # type: (float, Optional[Any]) -> None
        logger.debug("background worker got flush request")
        with self._lock:
            if self.is_alive and timeout > 0.0:
                self._wait_flush(timeout, callback)
        logger.debug("background worker flushed")

    def _wait_flush(self, timeout, callback):
        # type: (float, Optional[Any]) -> None
        initial_timeout = min(0.1, timeout)
        if not self._timed_queue_join(initial_timeout):
            pending = self._queue.qsize()
            logger.debug("%d event(s) pending on flush", pending)
            if callback is not None:
                callback(pending, timeout)
            self._timed_queue_join(timeout - initial_timeout)

    def submit(self, callback):
        # type: (Callable[[], None]) -> None
        self._ensure_thread()
        try:
            self._queue.put_nowait(callback)
        except queue.Full:
            logger.debug("background worker queue full, dropping event")

    def _target(self):
        # type: () -> None
        while True:
            callback = self._queue.get()
            try:
                if callback is _TERMINATOR:
                    break
                try:
                    callback()
                except Exception:
                    logger.error("Failed processing job", exc_info=True)
            finally:
                self._queue.task_done()
            sleep(0)
