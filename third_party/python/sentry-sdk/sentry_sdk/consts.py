from sentry_sdk._types import MYPY

if MYPY:
    from typing import Optional
    from typing import Callable
    from typing import Union
    from typing import List
    from typing import Type
    from typing import Dict
    from typing import Any
    from typing import Sequence
    from typing_extensions import TypedDict

    from sentry_sdk.transport import Transport
    from sentry_sdk.integrations import Integration

    from sentry_sdk._types import Event, EventProcessor, BreadcrumbProcessor

    # Experiments are feature flags to enable and disable certain unstable SDK
    # functionality. Changing them from the defaults (`None`) in production
    # code is highly discouraged. They are not subject to any stability
    # guarantees such as the ones from semantic versioning.
    Experiments = TypedDict(
        "Experiments",
        {
            "max_spans": Optional[int],
            "record_sql_params": Optional[bool],
            "auto_enabling_integrations": Optional[bool],
            "auto_session_tracking": Optional[bool],
        },
        total=False,
    )


# This type exists to trick mypy and PyCharm into thinking `init` and `Client`
# take these arguments (even though they take opaque **kwargs)
class ClientConstructor(object):
    def __init__(
        self,
        dsn=None,  # type: Optional[str]
        with_locals=True,  # type: bool
        max_breadcrumbs=100,  # type: int
        release=None,  # type: Optional[str]
        environment=None,  # type: Optional[str]
        server_name=None,  # type: Optional[str]
        shutdown_timeout=2,  # type: int
        integrations=[],  # type: Sequence[Integration]  # noqa: B006
        in_app_include=[],  # type: List[str]  # noqa: B006
        in_app_exclude=[],  # type: List[str]  # noqa: B006
        default_integrations=True,  # type: bool
        dist=None,  # type: Optional[str]
        transport=None,  # type: Optional[Union[Transport, Type[Transport], Callable[[Event], None]]]
        sample_rate=1.0,  # type: float
        send_default_pii=False,  # type: bool
        http_proxy=None,  # type: Optional[str]
        https_proxy=None,  # type: Optional[str]
        ignore_errors=[],  # type: List[Union[type, str]]  # noqa: B006
        request_bodies="medium",  # type: str
        before_send=None,  # type: Optional[EventProcessor]
        before_breadcrumb=None,  # type: Optional[BreadcrumbProcessor]
        debug=False,  # type: bool
        attach_stacktrace=False,  # type: bool
        ca_certs=None,  # type: Optional[str]
        propagate_traces=True,  # type: bool
        # DO NOT ENABLE THIS RIGHT NOW UNLESS YOU WANT TO EXCEED YOUR EVENT QUOTA IMMEDIATELY
        traces_sample_rate=0.0,  # type: float
        traceparent_v2=False,  # type: bool
        _experiments={},  # type: Experiments  # noqa: B006
    ):
        # type: (...) -> None
        pass


def _get_default_options():
    # type: () -> Dict[str, Any]
    import inspect

    if hasattr(inspect, "getfullargspec"):
        getargspec = inspect.getfullargspec
    else:
        getargspec = inspect.getargspec  # type: ignore

    a = getargspec(ClientConstructor.__init__)
    defaults = a.defaults or ()
    return dict(zip(a.args[-len(defaults) :], defaults))


DEFAULT_OPTIONS = _get_default_options()
del _get_default_options


VERSION = "0.14.3"
SDK_INFO = {
    "name": "sentry.python",
    "version": VERSION,
    "packages": [{"name": "pypi:sentry-sdk", "version": VERSION}],
}
