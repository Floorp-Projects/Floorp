.. _telemetry_pings:

=====================
Telemetry pings
=====================

A *Telemetry ping* is the data that we send to Mozillas Telemetry servers.

That data is stored as a JSON object client-side and contains common information to all pings and a payload specific to a certain *ping types*.

The top-level structure is defined by the :doc:`../data/common-ping` format.
It contains some basic information shared between different ping types, the :doc:`../data/environment` data (optional) and the data specific to the *ping type*, the *payload*.

Submission
==========

*Note:* The server-side behaviour is documented in the `HTTP Edge Server specification <https://wiki.mozilla.org/CloudServices/DataPipeline/HTTPEdgeServerSpecification>`_.

Pings are submitted via a common API on ``TelemetryController``.
If a ping fails to successfully submit to the server immediately (e.g. because
of missing internet connection), Telemetry will store it on disk and retry to
send it until the maximum ping age is exceeded (14 days).

*Note:* the :doc:`main pings <../data/main-ping>` are kept locally even after successful submission to enable the HealthReport and SelfSupport features. They will be deleted after their retention period of 180 days.

Sending of pending pings starts as soon as the delayed startup is finished. They are sent in batches, newest-first, with up
to 10 persisted pings per batch plus all unpersisted pings.
The send logic then waits for each batch to complete.

If it succeeds we trigger the next send of a ping batch. This is delayed as needed to only trigger one batch send per minute.

If ping sending encounters an error that means retrying later, a backoff timeout behavior is
triggered, exponentially increasing the timeout for the next try from 1 minute up to a limit of 120 minutes.
Any new ping submissions and "idle-daily" events reset this behavior as a safety mechanism and trigger immediate ping sending.

The telemetry server team is working towards `the common services status codes <https://wiki.mozilla.org/CloudServices/DataPipeline/HTTPEdgeServerSpecification#Server_Responses>`_, but for now the following logic is sufficient for Telemetry:

* `2XX` - success, don't resubmit
* `4XX` - there was some problem with the request - the client should not try to resubmit as it would just receive the same response
* `5XX` - there was a server-side error, the client should try to resubmit later

Ping types
==========

We send Telemetry with different ping types. The :doc:`main <../data/main-ping>` ping is the ping that contains the bulk of the Telemetry measurements for Firefox. For more specific use-cases, we also send other custom ping types.

Examples are:

* :doc:`main <../data/main-ping>` - contains the information collected by Telemetry (Histograms, hang stacks, ...)
* :doc:`saved-session <../data/main-ping>` - has the same format as a main ping, but it contains the *"classic"* Telemetry payload with measurements covering the whole browser session. This is only a separate type to make storage of saved-session easier server-side. This is temporary and will be removed soon.
* :doc:`crash <../data/crash-ping>` - a ping that is captured and sent after Firefox crashes.
* ``activation`` - *planned* - sent right after installation or profile creation
* ``upgrade`` - *planned* - sent right after an upgrade
* :doc:`deletion <../data/deletion-ping>` - sent when FHR upload is disabled, requesting deletion of the data associated with this user
* :doc:`uitour <../data/uitour-ping>` - a ping submitted via the UITour API
* :doc:`heartbeat <../data/heartbeat-ping>` - contains information on Heartbeat surveys
* :doc:`sync <../data/sync-ping>` - sent after a sync is completed or fails, contains information on sync errors and performance.

Pings sent from code that ships with Firefox are listed in the :doc:`data documentation <../data/index>`.

Archiving
=========

When archiving is enabled through the relative preference, pings submitted to ``TelemetryController`` are also stored locally in the user profile directory, in `<profile-dir>/datareporting/archived`.

To allow for cheaper lookup of archived pings, storage follows a specific naming scheme for both the directory and the ping file name: `<YYYY-MM>/<timestamp>.<UUID>.<type>.json`.

* ``<YYYY-MM>`` - The subdirectory name, generated from the ping creation date.
* ``<timestamp>`` - Timestamp of the ping creation date.
* ``<UUID>`` - The ping identifier.
* ``<type>`` - The ping type.
