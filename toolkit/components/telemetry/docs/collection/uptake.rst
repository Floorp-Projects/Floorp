================
Uptake Telemetry
================

Firefox continuously pulls data from different remote sources (eg. settings, system add-ons, …). In order to have consistent insights about the *uptake rate* of these *update sources*, our clients can use a unified Telemetry helper to report their *update status*.

The helper — described below — reports predefined update status, which eventually gives a unified way to obtain:

* the proportion of success among clients;
* its evolution over time;
* the distribution of error causes.

.. notes::

   Examples of update sources: *remote settings, addons update, addons, gfx, and plugins blocklists, certificate revocation, certificate pinning, system addons delivery…*

   Examples of update status: *up-to-date, success, network error, server error, signature error, server backoff, unknown error…*


Usage
-----

.. code-block:: js

   const { UptakeTelemetry } = Cu.import("resource://services-common/uptake-telemetry.js", {});

   UptakeTelemetry.report(source, status);

- ``source`` - a ``string`` that is an identifier for the update source (eg. ``addons-blocklist``)
- ``status`` - one of the following status constants:
  - ``UptakeTelemetry.STATUS.UP_TO_DATE``: Local content was already up-to-date with remote content.
  - ``UptakeTelemetry.STATUS.SUCCESS``: Local content was updated successfully.
  - ``UptakeTelemetry.STATUS.BACKOFF``: Remote server asked clients to backoff.
  - ``UptakeTelemetry.STATUS.PREF_DISABLED``: Update is disabled in user preferences.
  - ``UptakeTelemetry.STATUS.PARSE_ERROR``: Parsing server response has failed.
  - ``UptakeTelemetry.STATUS.CONTENT_ERROR``: Server response has unexpected content.
  - ``UptakeTelemetry.STATUS.SIGNATURE_ERROR``: Signature verification after diff-based sync has failed.
  - ``UptakeTelemetry.STATUS.SIGNATURE_RETRY_ERROR``: Signature verification after full fetch has failed.
  - ``UptakeTelemetry.STATUS.CONFLICT_ERROR``: Some remote changes are in conflict with local changes.
  - ``UptakeTelemetry.STATUS.SYNC_ERROR``: Synchronization of remote changes has failed.
  - ``UptakeTelemetry.STATUS.APPLY_ERROR``: Application of changes locally has failed.
  - ``UptakeTelemetry.STATUS.SERVER_ERROR``: Server failed to respond.
  - ``UptakeTelemetry.STATUS.CERTIFICATE_ERROR``: Server certificate verification has failed.
  - ``UptakeTelemetry.STATUS.DOWNLOAD_ERROR``: Data could not be fully retrieved.
  - ``UptakeTelemetry.STATUS.TIMEOUT_ERROR``: Server response has timed out.
  - ``UptakeTelemetry.STATUS.NETWORK_ERROR``: Communication with server has failed.
  - ``UptakeTelemetry.STATUS.NETWORK_OFFLINE_ERROR``: Network not available.
  - ``UptakeTelemetry.STATUS.CLEANUP_ERROR``: Clean-up of temporary files has failed.
  - ``UptakeTelemetry.STATUS.UNKNOWN_ERROR``: Uncategorized error.
  - ``UptakeTelemetry.STATUS.CUSTOM_1_ERROR``: Error #1 specific to this update source.
  - ``UptakeTelemetry.STATUS.CUSTOM_2_ERROR``: Error #2 specific to this update source.
  - ``UptakeTelemetry.STATUS.CUSTOM_3_ERROR``: Error #3 specific to this update source.
  - ``UptakeTelemetry.STATUS.CUSTOM_4_ERROR``: Error #4 specific to this update source.
  - ``UptakeTelemetry.STATUS.CUSTOM_5_ERROR``: Error #5 specific to this update source.


The data is submitted to a single :ref:`keyed histogram <histogram-type-keyed>` whose id is ``UPTAKE_REMOTE_CONTENT_RESULT_1`` and the specified update ``source`` as the key.

Example:

.. code-block:: js

   const UPDATE_SOURCE = "update-monitoring";

   let status;
   try {
     const data = await fetch(uri);
     status = UptakeTelemetry.STATUS.SUCCESS;
   } catch (e) {
     status = /NetworkError/.test(e) ?
                 UptakeTelemetry.STATUS.NETWORK_ERROR :
                 UptakeTelemetry.STATUS.SERVER_ERROR ;
   }
   UptakeTelemetry.report(UPDATE_SOURCE, status);


Use-cases
---------

The following remote data sources are already using this unified histogram.

* remote settings changes monitoring
* add-ons blocklist
* gfx blocklist
* plugins blocklist
* certificate revocation
* certificate pinning

Obviously, the goal is to eventually converge and avoid ad-hoc Telemetry probes for measuring uptake of remote content. Some notable potential use-cases are:

* nsUpdateService
* mozapps extensions update
* Shield recipe client
