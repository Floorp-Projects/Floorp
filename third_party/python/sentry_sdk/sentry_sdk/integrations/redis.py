from __future__ import absolute_import

from sentry_sdk import Hub
from sentry_sdk.utils import capture_internal_exceptions
from sentry_sdk.integrations import Integration

from sentry_sdk._types import MYPY

if MYPY:
    from typing import Any


class RedisIntegration(Integration):
    identifier = "redis"

    @staticmethod
    def setup_once():
        # type: () -> None
        import redis

        patch_redis_client(redis.StrictRedis)

        try:
            import rb.clients  # type: ignore
        except ImportError:
            pass
        else:
            patch_redis_client(rb.clients.FanoutClient)
            patch_redis_client(rb.clients.MappingClient)
            patch_redis_client(rb.clients.RoutingClient)


def patch_redis_client(cls):
    # type: (Any) -> None
    """
    This function can be used to instrument custom redis client classes or
    subclasses.
    """

    old_execute_command = cls.execute_command

    def sentry_patched_execute_command(self, name, *args, **kwargs):
        # type: (Any, str, *Any, **Any) -> Any
        hub = Hub.current

        if hub.get_integration(RedisIntegration) is None:
            return old_execute_command(self, name, *args, **kwargs)

        description = name

        with capture_internal_exceptions():
            description_parts = [name]
            for i, arg in enumerate(args):
                if i > 10:
                    break

                description_parts.append(repr(arg))

            description = " ".join(description_parts)

        with hub.start_span(op="redis", description=description) as span:
            if name:
                span.set_tag("redis.command", name)

            if name and args and name.lower() in ("get", "set", "setex", "setnx"):
                span.set_tag("redis.key", args[0])

            return old_execute_command(self, name, *args, **kwargs)

    cls.execute_command = sentry_patched_execute_command
