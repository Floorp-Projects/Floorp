from __future__ import absolute_import

import os
import sys
import weakref

from pyramid.httpexceptions import HTTPException
from pyramid.request import Request

from sentry_sdk.hub import Hub, _should_send_default_pii
from sentry_sdk.utils import capture_internal_exceptions, event_from_exception
from sentry_sdk._compat import reraise, iteritems

from sentry_sdk.integrations import Integration
from sentry_sdk.integrations._wsgi_common import RequestExtractor
from sentry_sdk.integrations.wsgi import SentryWsgiMiddleware

from sentry_sdk._types import MYPY

if MYPY:
    from pyramid.response import Response
    from typing import Any
    from sentry_sdk.integrations.wsgi import _ScopedResponse
    from typing import Callable
    from typing import Dict
    from typing import Optional
    from webob.cookies import RequestCookies  # type: ignore
    from webob.compat import cgi_FieldStorage  # type: ignore

    from sentry_sdk.utils import ExcInfo
    from sentry_sdk._types import EventProcessor


if getattr(Request, "authenticated_userid", None):

    def authenticated_userid(request):
        # type: (Request) -> Optional[Any]
        return request.authenticated_userid


else:
    # bw-compat for pyramid < 1.5
    from pyramid.security import authenticated_userid  # type: ignore


TRANSACTION_STYLE_VALUES = ("route_name", "route_pattern")


class PyramidIntegration(Integration):
    identifier = "pyramid"

    transaction_style = None

    def __init__(self, transaction_style="route_name"):
        # type: (str) -> None
        if transaction_style not in TRANSACTION_STYLE_VALUES:
            raise ValueError(
                "Invalid value for transaction_style: %s (must be in %s)"
                % (transaction_style, TRANSACTION_STYLE_VALUES)
            )
        self.transaction_style = transaction_style

    @staticmethod
    def setup_once():
        # type: () -> None
        from pyramid.router import Router
        from pyramid.request import Request

        old_handle_request = Router.handle_request

        def sentry_patched_handle_request(self, request, *args, **kwargs):
            # type: (Any, Request, *Any, **Any) -> Response
            hub = Hub.current
            integration = hub.get_integration(PyramidIntegration)
            if integration is not None:
                with hub.configure_scope() as scope:
                    scope.add_event_processor(
                        _make_event_processor(weakref.ref(request), integration)
                    )

            return old_handle_request(self, request, *args, **kwargs)

        Router.handle_request = sentry_patched_handle_request

        if hasattr(Request, "invoke_exception_view"):
            old_invoke_exception_view = Request.invoke_exception_view

            def sentry_patched_invoke_exception_view(self, *args, **kwargs):
                # type: (Request, *Any, **Any) -> Any
                rv = old_invoke_exception_view(self, *args, **kwargs)

                if (
                    self.exc_info
                    and all(self.exc_info)
                    and rv.status_int == 500
                    and Hub.current.get_integration(PyramidIntegration) is not None
                ):
                    _capture_exception(self.exc_info)

                return rv

            Request.invoke_exception_view = sentry_patched_invoke_exception_view

        old_wsgi_call = Router.__call__

        def sentry_patched_wsgi_call(self, environ, start_response):
            # type: (Any, Dict[str, str], Callable[..., Any]) -> _ScopedResponse
            hub = Hub.current
            integration = hub.get_integration(PyramidIntegration)
            if integration is None:
                return old_wsgi_call(self, environ, start_response)

            def sentry_patched_inner_wsgi_call(environ, start_response):
                # type: (Dict[str, Any], Callable[..., Any]) -> Any
                try:
                    return old_wsgi_call(self, environ, start_response)
                except Exception:
                    einfo = sys.exc_info()
                    _capture_exception(einfo)
                    reraise(*einfo)

            return SentryWsgiMiddleware(sentry_patched_inner_wsgi_call)(
                environ, start_response
            )

        Router.__call__ = sentry_patched_wsgi_call


def _capture_exception(exc_info):
    # type: (ExcInfo) -> None
    if exc_info[0] is None or issubclass(exc_info[0], HTTPException):
        return
    hub = Hub.current
    if hub.get_integration(PyramidIntegration) is None:
        return

    # If an integration is there, a client has to be there.
    client = hub.client  # type: Any

    event, hint = event_from_exception(
        exc_info,
        client_options=client.options,
        mechanism={"type": "pyramid", "handled": False},
    )

    hub.capture_event(event, hint=hint)


class PyramidRequestExtractor(RequestExtractor):
    def url(self):
        # type: () -> str
        return self.request.path_url

    def env(self):
        # type: () -> Dict[str, str]
        return self.request.environ

    def cookies(self):
        # type: () -> RequestCookies
        return self.request.cookies

    def raw_data(self):
        # type: () -> str
        return self.request.text

    def form(self):
        # type: () -> Dict[str, str]
        return {
            key: value
            for key, value in iteritems(self.request.POST)
            if not getattr(value, "filename", None)
        }

    def files(self):
        # type: () -> Dict[str, cgi_FieldStorage]
        return {
            key: value
            for key, value in iteritems(self.request.POST)
            if getattr(value, "filename", None)
        }

    def size_of_file(self, postdata):
        # type: (cgi_FieldStorage) -> int
        file = postdata.file
        try:
            return os.fstat(file.fileno()).st_size
        except Exception:
            return 0


def _make_event_processor(weak_request, integration):
    # type: (Callable[[], Request], PyramidIntegration) -> EventProcessor
    def event_processor(event, hint):
        # type: (Dict[str, Any], Dict[str, Any]) -> Dict[str, Any]
        request = weak_request()
        if request is None:
            return event

        try:
            if integration.transaction_style == "route_name":
                event["transaction"] = request.matched_route.name
            elif integration.transaction_style == "route_pattern":
                event["transaction"] = request.matched_route.pattern
        except Exception:
            pass

        with capture_internal_exceptions():
            PyramidRequestExtractor(request).extract_into_event(event)

        if _should_send_default_pii():
            with capture_internal_exceptions():
                user_info = event.setdefault("user", {})
                user_info.setdefault("id", authenticated_userid(request))

        return event

    return event_processor
