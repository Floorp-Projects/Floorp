.. _healthreport_architecture:

============
Architecture
============

``healthreporter.jsm`` contains the main interface for FHR, the
``HealthReporter`` type. An instance of this is created by the
``data_reporting_service`.

``providers.jsm`` contains numerous ``Metrics.Provider`` and
``Metrics.Measurement`` used for collecting application metrics. If you
are looking for the FHR probes, this is where they are.

Storage
=======

Firefox Health Report stores data in 3 locations:

* Metrics measurements and provider state is stored in a SQLite database
  (via ``Metrics.Storage``).
* Service state (such as the IDs of documents uploaded) is stored in a
  JSON file on disk (via OS.File).
* Lesser state and run-time options are stored in preferences.

Preferences
===========

Preferences controlling behavior of Firefox Health Report live in the
``datareporting.healthreport.*`` branch.

Service and Data Control
------------------------

The follow preferences control behavior of the service and data upload.

service.enabled
   Controls whether the entire health report service runs. The overall
   service performs data collection, storing, and submission.

   This is the primary kill switch for Firefox Health Report
   outside of the build system variable. i.e. if you are using an
   official Firefox build and wish to disable FHR, this is what you
   should set to false to prevent FHR from not only submitting but
   also collecting data.

uploadEnabled
   Whether uploading of data is enabled. This is the preference the
   checkbox in the preferences UI reflects. If this is
   disabled, FHR still collects data - it just doesn't upload it.

service.loadDelayMsec
   How long (in milliseconds) after initial application start should FHR
   wait before initializing.

   FHR may initialize sooner than this if the FHR service is requested.
   This will happen if e.g. the user goes to ``about:healthreport``.

service.loadDelayFirstRunMsec
   How long (in milliseconds) FHR should wait to initialize on first
   application run.

   FHR waits longer than normal to initialize on first application run
   because first-time initialization can use a lot of I/O to initialize
   the SQLite database and this I/O should not interfere with the
   first-run user experience.

documentServerURI
   The URI of a Bagheera server that FHR should interface with for
   submitting documents.

   You typically do not need to change this.

documentServerNamespace
   The namespace on the document server FHR should upload documents to.

   You typically do not need to change this.

infoURL
   The URL of a page containing more info about FHR, it's privacy
   policy, etc.

about.reportUrl
   The URL to load in ``about:healthreport``.

about.reportUrlUnified
   The URL to load in ``about:healthreport``. This is used instead of ``reportUrl`` for UnifiedTelemetry when it is not opt-in.

service.providerCategories
   A comma-delimited list of category manager categories that contain
   registered ``Metrics.Provider`` records. Read below for how provider
   registration works.

If the entire service is disabled, you lose data collection. This means
that **local** data analysis won't be available because there is no data
to analyze! Keep in mind that Firefox Health Report can be useful even
if it's not submitting data to remote servers!

Logging
-------

The following preferences allow you to control the logging behavior of
Firefox Health Report.

logging.consoleEnabled
   Whether to write log messages to the web console. This is true by
   default.

logging.consoleLevel
   The minimum log level FHR messages must have to be written to the
   web console. By default, only FHR warnings or errors will be written
   to the web console. During normal/expected operation, no messages of
   this type should be produced.

logging.dumpEnabled
   Whether to write log messages via ``dump()``. If true, FHR will write
   messages to stdout/stderr.

   This is typically only enabled when developing FHR.

logging.dumpLevel
   The minimum log level messages must have to be written via
   ``dump()``.

State
-----

currentDaySubmissionFailureCount
   How many submission failures the client has encountered while
   attempting to upload the most recent document.

lastDataSubmissionFailureTime
   The time of the last failed document upload.

lastDataSubmissionRequestedTime
   The time of the last document upload attempt.

lastDataSubmissionSuccessfulTime
   The time of the last successful document upload.

nextDataSubmissionTime
   The time the next data submission is scheduled for. FHR will not
   attempt to upload a new document before this time.

pendingDeleteRemoteData
   Whether the client currently has a pending request to delete remote
   data. If true, the client will attempt to delete all remote data
   before an upload is performed.

FHR stores various state in preferences.

Registering Providers
=====================

Firefox Health Report providers are registered via the category manager.
See ``HealthReportComponents.manifest`` for providers defined in this
directory.

Essentially, the category manager receives the name of a JS type and the
URI of a JSM to import that exports this symbol. At run-time, the
providers registered in the category manager are instantiated.

Providers are registered via the category manager to make registration
simple and less prone to errors. Any XPCOM component can create a
category manager entry. Therefore, new data providers can be added
without having to touch core Firefox Health Report code. Additionally,
category manager registration means providers are more likely to be
registered on FHR's terms, when it wants. If providers were registered
in code at application run-time, there would be the risk of other
components prematurely instantiating FHR (causing a performance hit if
performed at an inopportune time) or semi-complicated code around
observers or listeners. Category manager entries are only 1 line per
provider and leave FHR in control: they are simple and safe.

Document Generation and Lifecycle
=================================

FHR will attempt to submit a JSON document containing data every 24 wall
clock hours.

At upload time, FHR will query the database for **all** information from
the last 180 days and assemble this data into a JSON document. We
attempt to upload this JSON document with a client-generated UUID to the
configured server.

Before we attempt upload, the generated UUID is stored in the JSON state
file on local disk. At this point, the client assumes the document with
that UUID has been successfully stored on the server.

If the client is aware of other document UUIDs that presumably exist on
the server, those UUIDs are sent with the upload request so the client
can request those UUIDs be deleted. This helps ensure that each client
only has 1 document/UUID on the server at any one time.

Importance of Persisting UUIDs
------------------------------

The choices of how, where, and when document UUIDs are stored and updated
are very important. One should not attempt to change things unless she
has a very detailed understanding of why things are the way they are.

The client is purposefully very conservative about forgetting about
generated UUIDs. In other words, once a UUID is generated, the client
deliberately holds on to that UUID until it's very confident that UUID
is no longer stored on the server. The reason we do this is because
*orphaned* documents/UUIDs on the server can lead to faulty analysis,
such as over-reporting the number of Firefox installs that stop being
used.

When uploading a new UUID, we update the state and save the state file
to disk *before* an upload attempt because if the upload succeeds but
the response never makes it back to the client, we want the client to
know about the uploaded UUID so it can delete it later to prevent an
orphan.

We maintain a list of UUIDs locally (not simply the last UUID) because
multiple upload attempts could fail the same way as the previous
paragraph describes and we have no way of knowing which (if any)
actually succeeded. The safest approach is to assume every document
produced managed to get uploaded some how.

We store the UUIDs on a file on disk and not anywhere else because we
want storage to be robust. We originally stored UUIDs in preferences,
which only flush to disk periodically. Writes to preferences were
apparently getting lost. We switched to writing directly to files to
eliminate this window.
