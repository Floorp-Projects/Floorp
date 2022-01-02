from __future__ import absolute_import

import weakref

from sentry_sdk.hub import Hub
from sentry_sdk.integrations import Integration, DidNotEnable
from sentry_sdk.tracing import Span
from sentry_sdk.utils import capture_internal_exceptions, event_from_exception


try:
    from rq.version import VERSION as RQ_VERSION
    from rq.timeouts import JobTimeoutException
    from rq.worker import Worker
    from rq.queue import Queue
except ImportError:
    raise DidNotEnable("RQ not installed")

from sentry_sdk._types import MYPY

if MYPY:
    from typing import Any
    from typing import Dict
    from typing import Callable

    from rq.job import Job

    from sentry_sdk.utils import ExcInfo
    from sentry_sdk._types import EventProcessor


class RqIntegration(Integration):
    identifier = "rq"

    @staticmethod
    def setup_once():
        # type: () -> None

        try:
            version = tuple(map(int, RQ_VERSION.split(".")[:3]))
        except (ValueError, TypeError):
            raise DidNotEnable("Unparseable RQ version: {}".format(RQ_VERSION))

        if version < (0, 6):
            raise DidNotEnable("RQ 0.6 or newer is required.")

        old_perform_job = Worker.perform_job

        def sentry_patched_perform_job(self, job, *args, **kwargs):
            # type: (Any, Job, *Queue, **Any) -> bool
            hub = Hub.current
            integration = hub.get_integration(RqIntegration)

            if integration is None:
                return old_perform_job(self, job, *args, **kwargs)

            client = hub.client
            assert client is not None

            with hub.push_scope() as scope:
                scope.clear_breadcrumbs()
                scope.add_event_processor(_make_event_processor(weakref.ref(job)))

                span = Span.continue_from_headers(
                    job.meta.get("_sentry_trace_headers") or {}
                )
                span.op = "rq.task"

                with capture_internal_exceptions():
                    span.transaction = job.func_name

                with hub.start_span(span):
                    rv = old_perform_job(self, job, *args, **kwargs)

            if self.is_horse:
                # We're inside of a forked process and RQ is
                # about to call `os._exit`. Make sure that our
                # events get sent out.
                client.flush()

            return rv

        Worker.perform_job = sentry_patched_perform_job

        old_handle_exception = Worker.handle_exception

        def sentry_patched_handle_exception(self, job, *exc_info, **kwargs):
            # type: (Worker, Any, *Any, **Any) -> Any
            _capture_exception(exc_info)  # type: ignore
            return old_handle_exception(self, job, *exc_info, **kwargs)

        Worker.handle_exception = sentry_patched_handle_exception

        old_enqueue_job = Queue.enqueue_job

        def sentry_patched_enqueue_job(self, job, **kwargs):
            # type: (Queue, Any, **Any) -> Any
            hub = Hub.current
            if hub.get_integration(RqIntegration) is not None:
                job.meta["_sentry_trace_headers"] = dict(
                    hub.iter_trace_propagation_headers()
                )

            return old_enqueue_job(self, job, **kwargs)

        Queue.enqueue_job = sentry_patched_enqueue_job


def _make_event_processor(weak_job):
    # type: (Callable[[], Job]) -> EventProcessor
    def event_processor(event, hint):
        # type: (Dict[str, Any], Dict[str, Any]) -> Dict[str, Any]
        job = weak_job()
        if job is not None:
            with capture_internal_exceptions():
                extra = event.setdefault("extra", {})
                extra["rq-job"] = {
                    "job_id": job.id,
                    "func": job.func_name,
                    "args": job.args,
                    "kwargs": job.kwargs,
                    "description": job.description,
                }

        if "exc_info" in hint:
            with capture_internal_exceptions():
                if issubclass(hint["exc_info"][0], JobTimeoutException):
                    event["fingerprint"] = ["rq", "JobTimeoutException", job.func_name]

        return event

    return event_processor


def _capture_exception(exc_info, **kwargs):
    # type: (ExcInfo, **Any) -> None
    hub = Hub.current
    if hub.get_integration(RqIntegration) is None:
        return

    # If an integration is there, a client has to be there.
    client = hub.client  # type: Any

    event, hint = event_from_exception(
        exc_info,
        client_options=client.options,
        mechanism={"type": "rq", "handled": False},
    )

    hub.capture_event(event, hint=hint)
