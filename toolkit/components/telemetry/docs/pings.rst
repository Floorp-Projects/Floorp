.. _telemetry_pings:

=====================
Telemetry pings
=====================

A *Telemetry ping* is the data that we send to Mozillas Telemetry servers.

That data is stored as a JSON object client-side and contains common information to all pings and a payload specific to a certain *ping types*.

The top-level structure is defined by the :doc:`common-ping` format.
It contains some basic information shared between different ping types, the :doc:`environment` data (optional) and the data specific to the *ping type*, the *payload*.

Submission
==========

Pings are submitted via a common API on ``TelemetryPing``. It allows callers to choose a custom retention period that determines how long pings are kept on disk if submission wasn't successful.
If a ping failed to submit (e.g. because of missing internet connection), Telemetry will retry to submit it until its retention period is up.

*Note:* the :doc:`main pings <main-ping>` are kept locally even after successful submission to enable the HealthReport and SelfSupport features. They will be deleted after their retention period of 180 days.

Ping types
==========

* :doc:`main <main-ping>` - contains the information collected by Telemetry (Histograms, hang stacks, ...)
* ``activation`` - *planned* - sent right after installation or profile creation
* ``upgrade`` - *planned* - sent right after an upgrade
* ``deletion`` - *planned* - on opt-out we may have to tell the server to delete user data
