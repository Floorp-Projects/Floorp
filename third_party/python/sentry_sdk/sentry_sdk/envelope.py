import io
import json
import shutil
import mimetypes

from sentry_sdk._compat import text_type
from sentry_sdk._types import MYPY
from sentry_sdk.sessions import Session

if MYPY:
    from typing import Any
    from typing import Tuple
    from typing import Optional
    from typing import Union
    from typing import Dict
    from typing import List
    from typing import Iterator

    from sentry_sdk._types import Event, EventDataCategory


def get_event_data_category(event):
    # type: (Event) -> EventDataCategory
    if event.get("type") == "transaction":
        return "transaction"
    return "error"


class Envelope(object):
    def __init__(
        self,
        headers=None,  # type: Optional[Dict[str, str]]
        items=None,  # type: Optional[List[Item]]
    ):
        # type: (...) -> None
        if headers is not None:
            headers = dict(headers)
        self.headers = headers or {}
        if items is None:
            items = []
        else:
            items = list(items)
        self.items = items

    @property
    def description(self):
        # type: (...) -> str
        return "envelope with %s items (%s)" % (
            len(self.items),
            ", ".join(x.data_category for x in self.items),
        )

    def add_event(
        self, event  # type: Event
    ):
        # type: (...) -> None
        self.add_item(Item(payload=PayloadRef(json=event), type="event"))

    def add_session(
        self, session  # type: Union[Session, Any]
    ):
        # type: (...) -> None
        if isinstance(session, Session):
            session = session.to_json()
        self.add_item(Item(payload=PayloadRef(json=session), type="session"))

    def add_item(
        self, item  # type: Item
    ):
        # type: (...) -> None
        self.items.append(item)

    def get_event(self):
        # type: (...) -> Optional[Event]
        for items in self.items:
            event = items.get_event()
            if event is not None:
                return event
        return None

    def __iter__(self):
        # type: (...) -> Iterator[Item]
        return iter(self.items)

    def serialize_into(
        self, f  # type: Any
    ):
        # type: (...) -> None
        f.write(json.dumps(self.headers).encode("utf-8"))
        f.write(b"\n")
        for item in self.items:
            item.serialize_into(f)

    def serialize(self):
        # type: (...) -> bytes
        out = io.BytesIO()
        self.serialize_into(out)
        return out.getvalue()

    @classmethod
    def deserialize_from(
        cls, f  # type: Any
    ):
        # type: (...) -> Envelope
        headers = json.loads(f.readline())
        items = []
        while 1:
            item = Item.deserialize_from(f)
            if item is None:
                break
            items.append(item)
        return cls(headers=headers, items=items)

    @classmethod
    def deserialize(
        cls, bytes  # type: bytes
    ):
        # type: (...) -> Envelope
        return cls.deserialize_from(io.BytesIO(bytes))

    def __repr__(self):
        # type: (...) -> str
        return "<Envelope headers=%r items=%r>" % (self.headers, self.items)


class PayloadRef(object):
    def __init__(
        self,
        bytes=None,  # type: Optional[bytes]
        path=None,  # type: Optional[Union[bytes, text_type]]
        json=None,  # type: Optional[Any]
    ):
        # type: (...) -> None
        self.json = json
        self.bytes = bytes
        self.path = path

    def get_bytes(self):
        # type: (...) -> bytes
        if self.bytes is None:
            if self.path is not None:
                with open(self.path, "rb") as f:
                    self.bytes = f.read()
            elif self.json is not None:
                self.bytes = json.dumps(self.json).encode("utf-8")
            else:
                self.bytes = b""
        return self.bytes

    def _prepare_serialize(self):
        # type: (...) -> Tuple[Any, Any]
        if self.path is not None and self.bytes is None:
            f = open(self.path, "rb")
            f.seek(0, 2)
            length = f.tell()
            f.seek(0, 0)

            def writer(out):
                # type: (Any) -> None
                try:
                    shutil.copyfileobj(f, out)
                finally:
                    f.close()

            return length, writer

        bytes = self.get_bytes()
        return len(bytes), lambda f: f.write(bytes)

    @property
    def inferred_content_type(self):
        # type: (...) -> str
        if self.json is not None:
            return "application/json"
        elif self.path is not None:
            path = self.path
            if isinstance(path, bytes):
                path = path.decode("utf-8", "replace")
            ty = mimetypes.guess_type(path)[0]
            if ty:
                return ty
        return "application/octet-stream"

    def __repr__(self):
        # type: (...) -> str
        return "<Payload %r>" % (self.inferred_content_type,)


class Item(object):
    def __init__(
        self,
        payload,  # type: Union[bytes, text_type, PayloadRef]
        headers=None,  # type: Optional[Dict[str, str]]
        type=None,  # type: Optional[str]
        content_type=None,  # type: Optional[str]
        filename=None,  # type: Optional[str]
    ):
        if headers is not None:
            headers = dict(headers)
        elif headers is None:
            headers = {}
        self.headers = headers
        if isinstance(payload, bytes):
            payload = PayloadRef(bytes=payload)
        elif isinstance(payload, text_type):
            payload = PayloadRef(bytes=payload.encode("utf-8"))
        else:
            payload = payload

        if filename is not None:
            headers["filename"] = filename
        if type is not None:
            headers["type"] = type
        if content_type is not None:
            headers["content_type"] = content_type
        elif "content_type" not in headers:
            headers["content_type"] = payload.inferred_content_type

        self.payload = payload

    def __repr__(self):
        # type: (...) -> str
        return "<Item headers=%r payload=%r data_category=%r>" % (
            self.headers,
            self.payload,
            self.data_category,
        )

    @property
    def data_category(self):
        # type: (...) -> EventDataCategory
        rv = "default"  # type: Any
        event = self.get_event()
        if event is not None:
            rv = get_event_data_category(event)
        else:
            ty = self.headers.get("type")
            if ty in ("session", "attachment"):
                rv = ty
        return rv

    def get_bytes(self):
        # type: (...) -> bytes
        return self.payload.get_bytes()

    def get_event(self):
        # type: (...) -> Optional[Event]
        if self.headers.get("type") == "event" and self.payload.json is not None:
            return self.payload.json
        return None

    def serialize_into(
        self, f  # type: Any
    ):
        # type: (...) -> None
        headers = dict(self.headers)
        length, writer = self.payload._prepare_serialize()
        headers["length"] = length
        f.write(json.dumps(headers).encode("utf-8"))
        f.write(b"\n")
        writer(f)
        f.write(b"\n")

    def serialize(self):
        # type: (...) -> bytes
        out = io.BytesIO()
        self.serialize_into(out)
        return out.getvalue()

    @classmethod
    def deserialize_from(
        cls, f  # type: Any
    ):
        # type: (...) -> Optional[Item]
        line = f.readline().rstrip()
        if not line:
            return None
        headers = json.loads(line)
        length = headers["length"]
        payload = f.read(length)
        if headers.get("type") == "event":
            rv = cls(headers=headers, payload=PayloadRef(json=json.loads(payload)))
        else:
            rv = cls(headers=headers, payload=payload)
        f.readline()
        return rv

    @classmethod
    def deserialize(
        cls, bytes  # type: bytes
    ):
        # type: (...) -> Optional[Item]
        return cls.deserialize_from(io.BytesIO(bytes))
