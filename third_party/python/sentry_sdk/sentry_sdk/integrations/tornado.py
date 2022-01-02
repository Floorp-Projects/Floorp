import weakref
from inspect import iscoroutinefunction

from sentry_sdk.hub import Hub, _should_send_default_pii
from sentry_sdk.utils import (
    HAS_REAL_CONTEXTVARS,
    event_from_exception,
    capture_internal_exceptions,
    transaction_from_function,
)
from sentry_sdk.integrations import Integration, DidNotEnable
from sentry_sdk.integrations._wsgi_common import (
    RequestExtractor,
    _filter_headers,
    _is_json_content_type,
)
from sentry_sdk.integrations.logging import ignore_logger
from sentry_sdk._compat import iteritems

try:
    from tornado import version_info as TORNADO_VERSION  # type: ignore
    from tornado.web import RequestHandler, HTTPError
    from tornado.gen import coroutine
except ImportError:
    raise DidNotEnable("Tornado not installed")

from sentry_sdk._types import MYPY

if MYPY:
    from typing import Any
    from typing import Optional
    from typing import Dict
    from typing import Callable

    from sentry_sdk._types import EventProcessor


class TornadoIntegration(Integration):
    identifier = "tornado"

    @staticmethod
    def setup_once():
        # type: () -> None
        if TORNADO_VERSION < (5, 0):
            raise DidNotEnable("Tornado 5+ required")

        if not HAS_REAL_CONTEXTVARS:
            # Tornado is async. We better have contextvars or we're going to leak
            # state between requests.
            raise DidNotEnable(
                "The tornado integration for Sentry requires Python 3.6+ or the aiocontextvars package"
            )

        ignore_logger("tornado.access")

        old_execute = RequestHandler._execute  # type: ignore

        awaitable = iscoroutinefunction(old_execute)

        if awaitable:
            # Starting Tornado 6 RequestHandler._execute method is a standard Python coroutine (async/await)
            # In that case our method should be a coroutine function too
            async def sentry_execute_request_handler(self, *args, **kwargs):
                # type: (Any, *Any, **Any) -> Any
                hub = Hub.current
                integration = hub.get_integration(TornadoIntegration)
                if integration is None:
                    return await old_execute(self, *args, **kwargs)

                weak_handler = weakref.ref(self)

                with Hub(hub) as hub:
                    with hub.configure_scope() as scope:
                        scope.clear_breadcrumbs()
                        processor = _make_event_processor(weak_handler)  # type: ignore
                        scope.add_event_processor(processor)
                    return await old_execute(self, *args, **kwargs)

        else:

            @coroutine  # type: ignore
            def sentry_execute_request_handler(self, *args, **kwargs):
                # type: (RequestHandler, *Any, **Any) -> Any
                hub = Hub.current
                integration = hub.get_integration(TornadoIntegration)
                if integration is None:
                    return old_execute(self, *args, **kwargs)

                weak_handler = weakref.ref(self)

                with Hub(hub) as hub:
                    with hub.configure_scope() as scope:
                        scope.clear_breadcrumbs()
                        processor = _make_event_processor(weak_handler)  # type: ignore
                        scope.add_event_processor(processor)
                    result = yield from old_execute(self, *args, **kwargs)
                    return result

        RequestHandler._execute = sentry_execute_request_handler  # type: ignore

        old_log_exception = RequestHandler.log_exception

        def sentry_log_exception(self, ty, value, tb, *args, **kwargs):
            # type: (Any, type, BaseException, Any, *Any, **Any) -> Optional[Any]
            _capture_exception(ty, value, tb)
            return old_log_exception(self, ty, value, tb, *args, **kwargs)  # type: ignore

        RequestHandler.log_exception = sentry_log_exception  # type: ignore


def _capture_exception(ty, value, tb):
    # type: (type, BaseException, Any) -> None
    hub = Hub.current
    if hub.get_integration(TornadoIntegration) is None:
        return
    if isinstance(value, HTTPError):
        return

    # If an integration is there, a client has to be there.
    client = hub.client  # type: Any

    event, hint = event_from_exception(
        (ty, value, tb),
        client_options=client.options,
        mechanism={"type": "tornado", "handled": False},
    )

    hub.capture_event(event, hint=hint)


def _make_event_processor(weak_handler):
    # type: (Callable[[], RequestHandler]) -> EventProcessor
    def tornado_processor(event, hint):
        # type: (Dict[str, Any], Dict[str, Any]) -> Dict[str, Any]
        handler = weak_handler()
        if handler is None:
            return event

        request = handler.request

        with capture_internal_exceptions():
            method = getattr(handler, handler.request.method.lower())
            event["transaction"] = transaction_from_function(method)

        with capture_internal_exceptions():
            extractor = TornadoRequestExtractor(request)
            extractor.extract_into_event(event)

            request_info = event["request"]

            request_info["url"] = "%s://%s%s" % (
                request.protocol,
                request.host,
                request.path,
            )

            request_info["query_string"] = request.query
            request_info["method"] = request.method
            request_info["env"] = {"REMOTE_ADDR": request.remote_ip}
            request_info["headers"] = _filter_headers(dict(request.headers))

        with capture_internal_exceptions():
            if handler.current_user and _should_send_default_pii():
                event.setdefault("user", {}).setdefault("is_authenticated", True)

        return event

    return tornado_processor


class TornadoRequestExtractor(RequestExtractor):
    def content_length(self):
        # type: () -> int
        if self.request.body is None:
            return 0
        return len(self.request.body)

    def cookies(self):
        # type: () -> Dict[str, str]
        return {k: v.value for k, v in iteritems(self.request.cookies)}

    def raw_data(self):
        # type: () -> bytes
        return self.request.body

    def form(self):
        # type: () -> Dict[str, Any]
        return {
            k: [v.decode("latin1", "replace") for v in vs]
            for k, vs in iteritems(self.request.body_arguments)
        }

    def is_json(self):
        # type: () -> bool
        return _is_json_content_type(self.request.headers.get("content-type"))

    def files(self):
        # type: () -> Dict[str, Any]
        return {k: v[0] for k, v in iteritems(self.request.files) if v}

    def size_of_file(self, file):
        # type: (Any) -> int
        return len(file.body or ())
