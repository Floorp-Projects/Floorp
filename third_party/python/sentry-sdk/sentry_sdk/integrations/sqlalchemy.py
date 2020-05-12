from __future__ import absolute_import

from sentry_sdk._types import MYPY
from sentry_sdk.hub import Hub
from sentry_sdk.integrations import Integration, DidNotEnable
from sentry_sdk.tracing import record_sql_queries

try:
    from sqlalchemy.engine import Engine  # type: ignore
    from sqlalchemy.event import listen  # type: ignore
    from sqlalchemy import __version__ as SQLALCHEMY_VERSION  # type: ignore
except ImportError:
    raise DidNotEnable("SQLAlchemy not installed.")

if MYPY:
    from typing import Any
    from typing import ContextManager
    from typing import Optional

    from sentry_sdk.tracing import Span


class SqlalchemyIntegration(Integration):
    identifier = "sqlalchemy"

    @staticmethod
    def setup_once():
        # type: () -> None

        try:
            version = tuple(map(int, SQLALCHEMY_VERSION.split("b")[0].split(".")))
        except (TypeError, ValueError):
            raise DidNotEnable(
                "Unparseable SQLAlchemy version: {}".format(SQLALCHEMY_VERSION)
            )

        if version < (1, 2):
            raise DidNotEnable("SQLAlchemy 1.2 or newer required.")

        listen(Engine, "before_cursor_execute", _before_cursor_execute)
        listen(Engine, "after_cursor_execute", _after_cursor_execute)
        listen(Engine, "handle_error", _handle_error)


def _before_cursor_execute(
    conn, cursor, statement, parameters, context, executemany, *args
):
    # type: (Any, Any, Any, Any, Any, bool, *Any) -> None
    hub = Hub.current
    if hub.get_integration(SqlalchemyIntegration) is None:
        return

    ctx_mgr = record_sql_queries(
        hub,
        cursor,
        statement,
        parameters,
        paramstyle=context and context.dialect and context.dialect.paramstyle or None,
        executemany=executemany,
    )
    conn._sentry_sql_span_manager = ctx_mgr

    span = ctx_mgr.__enter__()

    if span is not None:
        conn._sentry_sql_span = span


def _after_cursor_execute(conn, cursor, statement, *args):
    # type: (Any, Any, Any, *Any) -> None
    ctx_mgr = getattr(
        conn, "_sentry_sql_span_manager", None
    )  # type: ContextManager[Any]

    if ctx_mgr is not None:
        conn._sentry_sql_span_manager = None
        ctx_mgr.__exit__(None, None, None)


def _handle_error(context, *args):
    # type: (Any, *Any) -> None
    conn = context.connection
    span = getattr(conn, "_sentry_sql_span", None)  # type: Optional[Span]

    if span is not None:
        span.set_status("internal_error")
