import inspect
from contextlib import contextmanager

from sentry_sdk.hub import Hub
from sentry_sdk.scope import Scope

from sentry_sdk._types import MYPY

if MYPY:
    from typing import Any
    from typing import Dict
    from typing import Optional
    from typing import overload
    from typing import Callable
    from typing import TypeVar
    from typing import ContextManager

    from sentry_sdk._types import Event, Hint, Breadcrumb, BreadcrumbHint
    from sentry_sdk.tracing import Span

    T = TypeVar("T")
    F = TypeVar("F", bound=Callable[..., Any])
else:

    def overload(x):
        # type: (T) -> T
        return x


__all__ = [
    "capture_event",
    "capture_message",
    "capture_exception",
    "add_breadcrumb",
    "configure_scope",
    "push_scope",
    "flush",
    "last_event_id",
    "start_span",
    "set_tag",
    "set_context",
    "set_extra",
    "set_user",
    "set_level",
]


def hubmethod(f):
    # type: (F) -> F
    f.__doc__ = "%s\n\n%s" % (
        "Alias for :py:meth:`sentry_sdk.Hub.%s`" % f.__name__,
        inspect.getdoc(getattr(Hub, f.__name__)),
    )
    return f


def scopemethod(f):
    # type: (F) -> F
    f.__doc__ = "%s\n\n%s" % (
        "Alias for :py:meth:`sentry_sdk.Scope.%s`" % f.__name__,
        inspect.getdoc(getattr(Scope, f.__name__)),
    )
    return f


@hubmethod
def capture_event(
    event,  # type: Event
    hint=None,  # type: Optional[Hint]
    scope=None,  # type: Optional[Any]
    **scope_args  # type: Dict[str, Any]
):
    # type: (...) -> Optional[str]
    hub = Hub.current
    if hub is not None:
        return hub.capture_event(event, hint, scope=scope, **scope_args)
    return None


@hubmethod
def capture_message(
    message,  # type: str
    level=None,  # type: Optional[str]
    scope=None,  # type: Optional[Any]
    **scope_args  # type: Dict[str, Any]
):
    # type: (...) -> Optional[str]
    hub = Hub.current
    if hub is not None:
        return hub.capture_message(message, level, scope=scope, **scope_args)
    return None


@hubmethod
def capture_exception(
    error=None,  # type: Optional[BaseException]
    scope=None,  # type: Optional[Any]
    **scope_args  # type: Dict[str, Any]
):
    # type: (...) -> Optional[str]
    hub = Hub.current
    if hub is not None:
        return hub.capture_exception(error, scope=scope, **scope_args)
    return None


@hubmethod
def add_breadcrumb(
    crumb=None,  # type: Optional[Breadcrumb]
    hint=None,  # type: Optional[BreadcrumbHint]
    **kwargs  # type: Any
):
    # type: (...) -> None
    hub = Hub.current
    if hub is not None:
        return hub.add_breadcrumb(crumb, hint, **kwargs)


@overload  # noqa
def configure_scope():
    # type: () -> ContextManager[Scope]
    pass


@overload  # noqa
def configure_scope(
    callback,  # type: Callable[[Scope], None]
):
    # type: (...) -> None
    pass


@hubmethod  # noqa
def configure_scope(
    callback=None,  # type: Optional[Callable[[Scope], None]]
):
    # type: (...) -> Optional[ContextManager[Scope]]
    hub = Hub.current
    if hub is not None:
        return hub.configure_scope(callback)
    elif callback is None:

        @contextmanager
        def inner():
            yield Scope()

        return inner()
    else:
        # returned if user provided callback
        return None


@overload  # noqa
def push_scope():
    # type: () -> ContextManager[Scope]
    pass


@overload  # noqa
def push_scope(
    callback,  # type: Callable[[Scope], None]
):
    # type: (...) -> None
    pass


@hubmethod  # noqa
def push_scope(
    callback=None,  # type: Optional[Callable[[Scope], None]]
):
    # type: (...) -> Optional[ContextManager[Scope]]
    hub = Hub.current
    if hub is not None:
        return hub.push_scope(callback)
    elif callback is None:

        @contextmanager
        def inner():
            yield Scope()

        return inner()
    else:
        # returned if user provided callback
        return None


@scopemethod  # noqa
def set_tag(key, value):
    # type: (str, Any) -> None
    hub = Hub.current
    if hub is not None:
        hub.scope.set_tag(key, value)


@scopemethod  # noqa
def set_context(key, value):
    # type: (str, Any) -> None
    hub = Hub.current
    if hub is not None:
        hub.scope.set_context(key, value)


@scopemethod  # noqa
def set_extra(key, value):
    # type: (str, Any) -> None
    hub = Hub.current
    if hub is not None:
        hub.scope.set_extra(key, value)


@scopemethod  # noqa
def set_user(value):
    # type: (Dict[str, Any]) -> None
    hub = Hub.current
    if hub is not None:
        hub.scope.set_user(value)


@scopemethod  # noqa
def set_level(value):
    # type: (str) -> None
    hub = Hub.current
    if hub is not None:
        hub.scope.set_level(value)


@hubmethod
def flush(
    timeout=None,  # type: Optional[float]
    callback=None,  # type: Optional[Callable[[int, float], None]]
):
    # type: (...) -> None
    hub = Hub.current
    if hub is not None:
        return hub.flush(timeout=timeout, callback=callback)


@hubmethod
def last_event_id():
    # type: () -> Optional[str]
    hub = Hub.current
    if hub is not None:
        return hub.last_event_id()
    return None


@hubmethod
def start_span(
    span=None,  # type: Optional[Span]
    **kwargs  # type: Any
):
    # type: (...) -> Span

    # TODO: All other functions in this module check for
    # `Hub.current is None`. That actually should never happen?
    return Hub.current.start_span(span=span, **kwargs)
