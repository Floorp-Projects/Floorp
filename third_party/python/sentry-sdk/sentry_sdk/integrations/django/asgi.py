"""
Instrumentation for Django 3.0

Since this file contains `async def` it is conditionally imported in
`sentry_sdk.integrations.django` (depending on the existence of
`django.core.handlers.asgi`.
"""

from sentry_sdk import Hub
from sentry_sdk._types import MYPY

from sentry_sdk.integrations.django import DjangoIntegration
from sentry_sdk.integrations.asgi import SentryAsgiMiddleware

if MYPY:
    from typing import Any


def patch_django_asgi_handler_impl(cls):
    # type: (Any) -> None
    old_app = cls.__call__

    async def sentry_patched_asgi_handler(self, scope, receive, send):
        # type: (Any, Any, Any, Any) -> Any
        if Hub.current.get_integration(DjangoIntegration) is None:
            return await old_app(self, scope, receive, send)

        middleware = SentryAsgiMiddleware(old_app.__get__(self, cls))._run_asgi3
        return await middleware(scope, receive, send)

    cls.__call__ = sentry_patched_asgi_handler


def patch_channels_asgi_handler_impl(cls):
    # type: (Any) -> None
    old_app = cls.__call__

    async def sentry_patched_asgi_handler(self, receive, send):
        # type: (Any, Any, Any) -> Any
        if Hub.current.get_integration(DjangoIntegration) is None:
            return await old_app(self, receive, send)

        middleware = SentryAsgiMiddleware(lambda _scope: old_app.__get__(self, cls))

        return await middleware(self.scope)(receive, send)

    cls.__call__ = sentry_patched_asgi_handler
