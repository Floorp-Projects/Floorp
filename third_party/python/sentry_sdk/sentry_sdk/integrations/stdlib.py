import os
import subprocess
import sys
import platform

from sentry_sdk.hub import Hub
from sentry_sdk.integrations import Integration
from sentry_sdk.scope import add_global_event_processor
from sentry_sdk.tracing import EnvironHeaders
from sentry_sdk.utils import capture_internal_exceptions, safe_repr

from sentry_sdk._types import MYPY

if MYPY:
    from typing import Any
    from typing import Callable
    from typing import Dict
    from typing import Optional
    from typing import List

    from sentry_sdk._types import Event, Hint


try:
    from httplib import HTTPConnection  # type: ignore
except ImportError:
    from http.client import HTTPConnection


_RUNTIME_CONTEXT = {
    "name": platform.python_implementation(),
    "version": "%s.%s.%s" % (sys.version_info[:3]),
    "build": sys.version,
}


class StdlibIntegration(Integration):
    identifier = "stdlib"

    @staticmethod
    def setup_once():
        # type: () -> None
        _install_httplib()
        _install_subprocess()

        @add_global_event_processor
        def add_python_runtime_context(event, hint):
            # type: (Event, Hint) -> Optional[Event]
            if Hub.current.get_integration(StdlibIntegration) is not None:
                contexts = event.setdefault("contexts", {})
                if isinstance(contexts, dict) and "runtime" not in contexts:
                    contexts["runtime"] = _RUNTIME_CONTEXT

            return event


def _install_httplib():
    # type: () -> None
    real_putrequest = HTTPConnection.putrequest
    real_getresponse = HTTPConnection.getresponse

    def putrequest(self, method, url, *args, **kwargs):
        # type: (HTTPConnection, str, str, *Any, **Any) -> Any
        hub = Hub.current
        if hub.get_integration(StdlibIntegration) is None:
            return real_putrequest(self, method, url, *args, **kwargs)

        host = self.host
        port = self.port
        default_port = self.default_port

        real_url = url
        if not real_url.startswith(("http://", "https://")):
            real_url = "%s://%s%s%s" % (
                default_port == 443 and "https" or "http",
                host,
                port != default_port and ":%s" % port or "",
                url,
            )

        span = hub.start_span(op="http", description="%s %s" % (method, real_url))

        span.set_data("method", method)
        span.set_data("url", real_url)

        rv = real_putrequest(self, method, url, *args, **kwargs)

        for key, value in hub.iter_trace_propagation_headers():
            self.putheader(key, value)

        self._sentrysdk_span = span

        return rv

    def getresponse(self, *args, **kwargs):
        # type: (HTTPConnection, *Any, **Any) -> Any
        span = getattr(self, "_sentrysdk_span", None)

        if span is None:
            return real_getresponse(self, *args, **kwargs)

        rv = real_getresponse(self, *args, **kwargs)

        span.set_data("status_code", rv.status)
        span.set_http_status(int(rv.status))
        span.set_data("reason", rv.reason)
        span.finish()

        return rv

    HTTPConnection.putrequest = putrequest
    HTTPConnection.getresponse = getresponse


def _init_argument(args, kwargs, name, position, setdefault_callback=None):
    # type: (List[Any], Dict[Any, Any], str, int, Optional[Callable[[Any], Any]]) -> Any
    """
    given (*args, **kwargs) of a function call, retrieve (and optionally set a
    default for) an argument by either name or position.

    This is useful for wrapping functions with complex type signatures and
    extracting a few arguments without needing to redefine that function's
    entire type signature.
    """

    if name in kwargs:
        rv = kwargs[name]
        if setdefault_callback is not None:
            rv = setdefault_callback(rv)
        if rv is not None:
            kwargs[name] = rv
    elif position < len(args):
        rv = args[position]
        if setdefault_callback is not None:
            rv = setdefault_callback(rv)
        if rv is not None:
            args[position] = rv
    else:
        rv = setdefault_callback and setdefault_callback(None)
        if rv is not None:
            kwargs[name] = rv

    return rv


def _install_subprocess():
    # type: () -> None
    old_popen_init = subprocess.Popen.__init__

    def sentry_patched_popen_init(self, *a, **kw):
        # type: (subprocess.Popen[Any], *Any, **Any) -> None

        hub = Hub.current
        if hub.get_integration(StdlibIntegration) is None:
            return old_popen_init(self, *a, **kw)  # type: ignore

        # Convert from tuple to list to be able to set values.
        a = list(a)

        args = _init_argument(a, kw, "args", 0) or []
        cwd = _init_argument(a, kw, "cwd", 9)

        # if args is not a list or tuple (and e.g. some iterator instead),
        # let's not use it at all. There are too many things that can go wrong
        # when trying to collect an iterator into a list and setting that list
        # into `a` again.
        #
        # Also invocations where `args` is not a sequence are not actually
        # legal. They just happen to work under CPython.
        description = None

        if isinstance(args, (list, tuple)) and len(args) < 100:
            with capture_internal_exceptions():
                description = " ".join(map(str, args))

        if description is None:
            description = safe_repr(args)

        env = None

        for k, v in hub.iter_trace_propagation_headers():
            if env is None:
                env = _init_argument(a, kw, "env", 10, lambda x: dict(x or os.environ))
            env["SUBPROCESS_" + k.upper().replace("-", "_")] = v

        with hub.start_span(op="subprocess", description=description) as span:
            if cwd:
                span.set_data("subprocess.cwd", cwd)

            rv = old_popen_init(self, *a, **kw)  # type: ignore

            span.set_tag("subprocess.pid", self.pid)
            return rv

    subprocess.Popen.__init__ = sentry_patched_popen_init  # type: ignore

    old_popen_wait = subprocess.Popen.wait

    def sentry_patched_popen_wait(self, *a, **kw):
        # type: (subprocess.Popen[Any], *Any, **Any) -> Any
        hub = Hub.current

        if hub.get_integration(StdlibIntegration) is None:
            return old_popen_wait(self, *a, **kw)

        with hub.start_span(op="subprocess.wait") as span:
            span.set_tag("subprocess.pid", self.pid)
            return old_popen_wait(self, *a, **kw)

    subprocess.Popen.wait = sentry_patched_popen_wait  # type: ignore

    old_popen_communicate = subprocess.Popen.communicate

    def sentry_patched_popen_communicate(self, *a, **kw):
        # type: (subprocess.Popen[Any], *Any, **Any) -> Any
        hub = Hub.current

        if hub.get_integration(StdlibIntegration) is None:
            return old_popen_communicate(self, *a, **kw)

        with hub.start_span(op="subprocess.communicate") as span:
            span.set_tag("subprocess.pid", self.pid)
            return old_popen_communicate(self, *a, **kw)

    subprocess.Popen.communicate = sentry_patched_popen_communicate  # type: ignore


def get_subprocess_traceparent_headers():
    # type: () -> EnvironHeaders
    return EnvironHeaders(os.environ, prefix="SUBPROCESS_")
