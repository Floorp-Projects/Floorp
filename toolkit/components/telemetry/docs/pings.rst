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

The telemetry server team is working towards `the common services status codes <https://wiki.mozilla.org/CloudServices/DataPipeline/HTTPEdgeServerSpecification#Server_Responses>`_, but for now the following logic is sufficient for Telemetry:

* `2XX` - success, don't resubmit
* `4XX` - there was some problem with the request - the client should not try to resubmit as it would just receive the same response
* `5XX` - there was a server-side error, the client should try to resubmit later

Ping types
==========

* :doc:`main <main-ping>` - contains the information collected by Telemetry (Histograms, hang stacks, ...)
* :doc:`saved-session <main-ping>` - has the same format as a main ping, but it contains the *"classic"* Telemetry payload with measurements covering the whole browser session. This is only a separate type to make storage of saved-session easier server-side.
* ``activation`` - *planned* - sent right after installation or profile creation
* ``upgrade`` - *planned* - sent right after an upgrade
* ``deletion`` - *planned* - on opt-out we may have to tell the server to delete user data

Archiving
=========

When archiving is enabled through the relative preference, pings submitted to ``TelemetryPing`` are also stored locally in the user profile directory, in `<profile-dir>/datareporting/archived`.

To allow for cheaper lookup of archived pings, storage follows a specific naming scheme for both the directory and the ping file name: `<YYYY-MM>/<timestamp>.<UUID>.<type>.json`.

* ``<YYYY-MM>`` - The subdirectory name, generated from the ping creation date.
* ``<timestamp>`` - Timestamp of the ping creation date.
* ``<UUID>`` - The ping identifier.
* ``<type>`` - The ping type.

Preferences
===========

Telemetry behaviour is controlled through the following preferences:

* ``datareporting.healthreport.service.enabled`` - If true, records base Telemetry data. Otherwise, completely disables telemetry recording.
* ``toolkit.telemetry.enabled`` - If true, record the extended Telemetry data. Please note that base Telemetry data needs to be enabled as well and we need to be in an official build or in test mode. This preference is controlled through the `Preferences` dialog.
* ``datareporting.healthreport.uploadEnabled`` - Send the data we record if user has consented to FHR. This preference is controlled through the `Preferences` dialog.
* ``toolkit.telemetry.archive.enabled`` - Allow pings to be archived locally.
