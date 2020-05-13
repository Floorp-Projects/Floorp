from datetime import datetime, timedelta
from os import environ
import sys

from sentry_sdk.hub import Hub, _should_send_default_pii
from sentry_sdk._compat import reraise
from sentry_sdk.utils import (
    AnnotatedValue,
    capture_internal_exceptions,
    event_from_exception,
    logger,
)
from sentry_sdk.integrations import Integration
from sentry_sdk.integrations._wsgi_common import _filter_headers

from sentry_sdk._types import MYPY

if MYPY:
    from typing import Any
    from typing import TypeVar
    from typing import Callable
    from typing import Optional

    from sentry_sdk._types import EventProcessor, Event, Hint

    F = TypeVar("F", bound=Callable[..., Any])


def _wrap_handler(handler):
    # type: (F) -> F
    def sentry_handler(event, context, *args, **kwargs):
        # type: (Any, Any, *Any, **Any) -> Any
        hub = Hub.current
        integration = hub.get_integration(AwsLambdaIntegration)
        if integration is None:
            return handler(event, context, *args, **kwargs)

        # If an integration is there, a client has to be there.
        client = hub.client  # type: Any

        with hub.push_scope() as scope:
            with capture_internal_exceptions():
                scope.clear_breadcrumbs()
                scope.transaction = context.function_name
                scope.add_event_processor(_make_request_event_processor(event, context))

            try:
                return handler(event, context, *args, **kwargs)
            except Exception:
                exc_info = sys.exc_info()
                event, hint = event_from_exception(
                    exc_info,
                    client_options=client.options,
                    mechanism={"type": "aws_lambda", "handled": False},
                )
                hub.capture_event(event, hint=hint)
                reraise(*exc_info)

    return sentry_handler  # type: ignore


def _drain_queue():
    # type: () -> None
    with capture_internal_exceptions():
        hub = Hub.current
        integration = hub.get_integration(AwsLambdaIntegration)
        if integration is not None:
            # Flush out the event queue before AWS kills the
            # process.
            hub.flush()


class AwsLambdaIntegration(Integration):
    identifier = "aws_lambda"

    @staticmethod
    def setup_once():
        # type: () -> None
        import __main__ as lambda_bootstrap  # type: ignore

        pre_37 = True  # Python 3.6 or 2.7

        if not hasattr(lambda_bootstrap, "handle_http_request"):
            try:
                import bootstrap as lambda_bootstrap  # type: ignore

                pre_37 = False  # Python 3.7
            except ImportError:
                pass

        if not hasattr(lambda_bootstrap, "handle_event_request"):
            logger.warning(
                "Not running in AWS Lambda environment, "
                "AwsLambdaIntegration disabled"
            )
            return

        if pre_37:
            old_handle_event_request = lambda_bootstrap.handle_event_request

            def sentry_handle_event_request(request_handler, *args, **kwargs):
                # type: (Any, *Any, **Any) -> Any
                request_handler = _wrap_handler(request_handler)
                return old_handle_event_request(request_handler, *args, **kwargs)

            lambda_bootstrap.handle_event_request = sentry_handle_event_request

            old_handle_http_request = lambda_bootstrap.handle_http_request

            def sentry_handle_http_request(request_handler, *args, **kwargs):
                # type: (Any, *Any, **Any) -> Any
                request_handler = _wrap_handler(request_handler)
                return old_handle_http_request(request_handler, *args, **kwargs)

            lambda_bootstrap.handle_http_request = sentry_handle_http_request

            # Patch to_json to drain the queue. This should work even when the
            # SDK is initialized inside of the handler

            old_to_json = lambda_bootstrap.to_json

            def sentry_to_json(*args, **kwargs):
                # type: (*Any, **Any) -> Any
                _drain_queue()
                return old_to_json(*args, **kwargs)

            lambda_bootstrap.to_json = sentry_to_json
        else:
            old_handle_event_request = lambda_bootstrap.handle_event_request

            def sentry_handle_event_request(  # type: ignore
                lambda_runtime_client, request_handler, *args, **kwargs
            ):
                request_handler = _wrap_handler(request_handler)
                return old_handle_event_request(
                    lambda_runtime_client, request_handler, *args, **kwargs
                )

            lambda_bootstrap.handle_event_request = sentry_handle_event_request

            # Patch the runtime client to drain the queue. This should work
            # even when the SDK is initialized inside of the handler

            def _wrap_post_function(f):
                # type: (F) -> F
                def inner(*args, **kwargs):
                    # type: (*Any, **Any) -> Any
                    _drain_queue()
                    return f(*args, **kwargs)

                return inner  # type: ignore

            lambda_bootstrap.LambdaRuntimeClient.post_invocation_result = _wrap_post_function(
                lambda_bootstrap.LambdaRuntimeClient.post_invocation_result
            )
            lambda_bootstrap.LambdaRuntimeClient.post_invocation_error = _wrap_post_function(
                lambda_bootstrap.LambdaRuntimeClient.post_invocation_error
            )


def _make_request_event_processor(aws_event, aws_context):
    # type: (Any, Any) -> EventProcessor
    start_time = datetime.now()

    def event_processor(event, hint, start_time=start_time):
        # type: (Event, Hint, datetime) -> Optional[Event]
        extra = event.setdefault("extra", {})
        extra["lambda"] = {
            "function_name": aws_context.function_name,
            "function_version": aws_context.function_version,
            "invoked_function_arn": aws_context.invoked_function_arn,
            "remaining_time_in_millis": aws_context.get_remaining_time_in_millis(),
            "aws_request_id": aws_context.aws_request_id,
        }

        extra["cloudwatch logs"] = {
            "url": _get_cloudwatch_logs_url(aws_context, start_time),
            "log_group": aws_context.log_group_name,
            "log_stream": aws_context.log_stream_name,
        }

        request = event.get("request", {})

        if "httpMethod" in aws_event:
            request["method"] = aws_event["httpMethod"]

        request["url"] = _get_url(aws_event, aws_context)

        if "queryStringParameters" in aws_event:
            request["query_string"] = aws_event["queryStringParameters"]

        if "headers" in aws_event:
            request["headers"] = _filter_headers(aws_event["headers"])

        if aws_event.get("body", None):
            # Unfortunately couldn't find a way to get structured body from AWS
            # event. Meaning every body is unstructured to us.
            request["data"] = AnnotatedValue("", {"rem": [["!raw", "x", 0, 0]]})

        if _should_send_default_pii():
            user_info = event.setdefault("user", {})

            id = aws_event.get("identity", {}).get("userArn")
            if id is not None:
                user_info.setdefault("id", id)

            ip = aws_event.get("identity", {}).get("sourceIp")
            if ip is not None:
                user_info.setdefault("ip_address", ip)

        event["request"] = request

        return event

    return event_processor


def _get_url(event, context):
    # type: (Any, Any) -> str
    path = event.get("path", None)
    headers = event.get("headers", {})
    host = headers.get("Host", None)
    proto = headers.get("X-Forwarded-Proto", None)
    if proto and host and path:
        return "{}://{}{}".format(proto, host, path)
    return "awslambda:///{}".format(context.function_name)


def _get_cloudwatch_logs_url(context, start_time):
    # type: (Any, datetime) -> str
    """
    Generates a CloudWatchLogs console URL based on the context object

    Arguments:
        context {Any} -- context from lambda handler

    Returns:
        str -- AWS Console URL to logs.
    """
    formatstring = "%Y-%m-%dT%H:%M:%S"

    url = (
        "https://console.aws.amazon.com/cloudwatch/home?region={region}"
        "#logEventViewer:group={log_group};stream={log_stream}"
        ";start={start_time};end={end_time}"
    ).format(
        region=environ.get("AWS_REGION"),
        log_group=context.log_group_name,
        log_stream=context.log_stream_name,
        start_time=(start_time - timedelta(seconds=1)).strftime(formatstring),
        end_time=(datetime.now() + timedelta(seconds=2)).strftime(formatstring),
    )

    return url
