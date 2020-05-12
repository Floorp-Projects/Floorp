from sentry_sdk.hub import Hub, init
from sentry_sdk.scope import Scope
from sentry_sdk.transport import Transport, HttpTransport
from sentry_sdk.client import Client

from sentry_sdk.api import *  # noqa
from sentry_sdk.api import __all__ as api_all

from sentry_sdk.consts import VERSION  # noqa

__all__ = api_all + [  # noqa
    "Hub",
    "Scope",
    "Client",
    "Transport",
    "HttpTransport",
    "init",
    "integrations",
]

# Initialize the debug support after everything is loaded
from sentry_sdk.debug import init_debug_support

init_debug_support()
del init_debug_support
