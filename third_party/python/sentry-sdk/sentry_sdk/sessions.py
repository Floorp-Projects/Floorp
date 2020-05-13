import os
import uuid
import time
from datetime import datetime
from threading import Thread, Lock
from contextlib import contextmanager

from sentry_sdk._types import MYPY
from sentry_sdk.utils import format_timestamp

if MYPY:
    import sentry_sdk

    from typing import Optional
    from typing import Union
    from typing import Any
    from typing import Dict
    from typing import Generator

    from sentry_sdk._types import SessionStatus


def is_auto_session_tracking_enabled(hub=None):
    # type: (Optional[sentry_sdk.Hub]) -> bool
    """Utility function to find out if session tracking is enabled."""
    if hub is None:
        hub = sentry_sdk.Hub.current
    should_track = hub.scope._force_auto_session_tracking
    if should_track is None:
        exp = hub.client.options["_experiments"] if hub.client else {}
        should_track = exp.get("auto_session_tracking")
    return should_track


@contextmanager
def auto_session_tracking(hub=None):
    # type: (Optional[sentry_sdk.Hub]) -> Generator[None, None, None]
    """Starts and stops a session automatically around a block."""
    if hub is None:
        hub = sentry_sdk.Hub.current
    should_track = is_auto_session_tracking_enabled(hub)
    if should_track:
        hub.start_session()
    try:
        yield
    finally:
        if should_track:
            hub.end_session()


def _make_uuid(
    val,  # type: Union[str, uuid.UUID]
):
    # type: (...) -> uuid.UUID
    if isinstance(val, uuid.UUID):
        return val
    return uuid.UUID(val)


TERMINAL_SESSION_STATES = ("exited", "abnormal", "crashed")


class SessionFlusher(object):
    def __init__(
        self,
        flush_func,  # type: Any
        flush_interval=10,  # type: int
    ):
        # type: (...) -> None
        self.flush_func = flush_func
        self.flush_interval = flush_interval
        self.pending = {}  # type: Dict[str, Any]
        self._thread = None  # type: Optional[Thread]
        self._thread_lock = Lock()
        self._thread_for_pid = None  # type: Optional[int]
        self._running = True

    def flush(self):
        # type: (...) -> None
        pending = self.pending
        self.pending = {}
        self.flush_func(list(pending.values()))

    def _ensure_running(self):
        # type: (...) -> None
        if self._thread_for_pid == os.getpid() and self._thread is not None:
            return None
        with self._thread_lock:
            if self._thread_for_pid == os.getpid() and self._thread is not None:
                return None

            def _thread():
                # type: (...) -> None
                while self._running:
                    time.sleep(self.flush_interval)
                    if self.pending and self._running:
                        self.flush()

            thread = Thread(target=_thread)
            thread.daemon = True
            thread.start()
            self._thread = thread
            self._thread_for_pid = os.getpid()
        return None

    def add_session(
        self, session  # type: Session
    ):
        # type: (...) -> None
        self.pending[session.sid.hex] = session.to_json()
        self._ensure_running()

    def kill(self):
        # type: (...) -> None
        self._running = False

    def __del__(self):
        # type: (...) -> None
        self.kill()


class Session(object):
    def __init__(
        self,
        sid=None,  # type: Optional[Union[str, uuid.UUID]]
        did=None,  # type: Optional[str]
        timestamp=None,  # type: Optional[datetime]
        started=None,  # type: Optional[datetime]
        duration=None,  # type: Optional[float]
        status=None,  # type: Optional[SessionStatus]
        release=None,  # type: Optional[str]
        environment=None,  # type: Optional[str]
        user_agent=None,  # type: Optional[str]
        ip_address=None,  # type: Optional[str]
        errors=None,  # type: Optional[int]
        user=None,  # type: Optional[Any]
    ):
        # type: (...) -> None
        if sid is None:
            sid = uuid.uuid4()
        if started is None:
            started = datetime.utcnow()
        if status is None:
            status = "ok"
        self.status = status
        self.did = None  # type: Optional[str]
        self.started = started
        self.release = None  # type: Optional[str]
        self.environment = None  # type: Optional[str]
        self.duration = None  # type: Optional[float]
        self.user_agent = None  # type: Optional[str]
        self.ip_address = None  # type: Optional[str]
        self.errors = 0

        self.update(
            sid=sid,
            did=did,
            timestamp=timestamp,
            duration=duration,
            release=release,
            environment=environment,
            user_agent=user_agent,
            ip_address=ip_address,
            errors=errors,
            user=user,
        )

    def update(
        self,
        sid=None,  # type: Optional[Union[str, uuid.UUID]]
        did=None,  # type: Optional[str]
        timestamp=None,  # type: Optional[datetime]
        duration=None,  # type: Optional[float]
        status=None,  # type: Optional[SessionStatus]
        release=None,  # type: Optional[str]
        environment=None,  # type: Optional[str]
        user_agent=None,  # type: Optional[str]
        ip_address=None,  # type: Optional[str]
        errors=None,  # type: Optional[int]
        user=None,  # type: Optional[Any]
    ):
        # type: (...) -> None
        # If a user is supplied we pull some data form it
        if user:
            if ip_address is None:
                ip_address = user.get("ip_address")
            if did is None:
                did = user.get("id") or user.get("email") or user.get("username")

        if sid is not None:
            self.sid = _make_uuid(sid)
        if did is not None:
            self.did = str(did)
        if timestamp is None:
            timestamp = datetime.utcnow()
        self.timestamp = timestamp
        if duration is not None:
            self.duration = duration
        if release is not None:
            self.release = release
        if environment is not None:
            self.environment = environment
        if ip_address is not None:
            self.ip_address = ip_address
        if user_agent is not None:
            self.user_agent = user_agent
        if errors is not None:
            self.errors = errors

        if status is not None:
            self.status = status

    def close(
        self, status=None  # type: Optional[SessionStatus]
    ):
        # type: (...) -> Any
        if status is None and self.status == "ok":
            status = "exited"
        if status is not None:
            self.update(status=status)

    def to_json(self):
        # type: (...) -> Any
        rv = {
            "sid": str(self.sid),
            "init": True,
            "started": format_timestamp(self.started),
            "timestamp": format_timestamp(self.timestamp),
            "status": self.status,
        }  # type: Dict[str, Any]
        if self.errors:
            rv["errors"] = self.errors
        if self.did is not None:
            rv["did"] = self.did
        if self.duration is not None:
            rv["duration"] = self.duration

        attrs = {}
        if self.release is not None:
            attrs["release"] = self.release
        if self.environment is not None:
            attrs["environment"] = self.environment
        if self.ip_address is not None:
            attrs["ip_address"] = self.ip_address
        if self.user_agent is not None:
            attrs["user_agent"] = self.user_agent
        if attrs:
            rv["attrs"] = attrs
        return rv
