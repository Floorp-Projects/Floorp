==========
Submission
==========

*Note:* The server-side behaviour is documented in the `HTTP Edge Server specification <https://wiki.mozilla.org/CloudServices/DataPipeline/HTTPEdgeServerSpecification>`_.

Pings are submitted via a common API on ``TelemetryController``.
If a ping fails to successfully submit to the server immediately (e.g. because
of missing internet connection), Telemetry will store it on disk and retry to
send it until the maximum ping age is exceeded (14 days).

.. note::

  The :doc:`main pings <../data/main-ping>` are kept locally even after successful submission to enable the HealthReport feature. They will be deleted after their retention period of 180 days.

Submission logic
================

Sending of pending pings starts as soon as the delayed startup is finished. They are sent in batches, newest-first, with up
to 10 persisted pings per batch plus all unpersisted pings.
The send logic then waits for each batch to complete.

If it succeeds we trigger the next send of a ping batch. This is delayed as needed to only trigger one batch send per minute.

If ping sending encounters an error that means retrying later, a backoff timeout behavior is
triggered, exponentially increasing the timeout for the next try from 1 minute up to a limit of 120 minutes.
Any new ping submissions and "idle-daily" events reset this behavior as a safety mechanism and trigger immediate ping sending.

Pingsender
==========
Some pings (e.g. :doc:`crash pings <../data/crash-ping>` and :doc:`main pings <../data/main-ping>` with reason `shutdown`) are submitted using the :doc:`../internals/pingsender`.

The pingsender tries to send each ping once and, if it fails, no additional attempt is performed: ``TelemetrySend`` will take care of retrying using the previously described submission logic.

Status codes
============

The telemetry server team is working towards `the common services status codes <https://wiki.mozilla.org/CloudServices/DataPipeline/HTTPEdgeServerSpecification#Server_Responses>`_, but for now the following logic is sufficient for Telemetry:

* `2XX` - success, don't resubmit
* `4XX` - there was some problem with the request - the client should not try to resubmit as it would just receive the same response
* `5XX` - there was a server-side error, the client should try to resubmit later
