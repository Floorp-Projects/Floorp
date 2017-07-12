.. _telemetry_pings:

=====================
Telemetry pings
=====================

A *Telemetry ping* is the data that we send to Mozillas Telemetry servers.

That data is stored as a JSON object client-side and contains common information to all pings and a payload specific to a certain *ping types*.

The top-level structure is defined by the :doc:`common ping format <../data/common-ping>` format.
It contains:

* some basic information shared between different ping types
* the :doc:`environment data <../data/environment>` (optional)
* the data specific to the *ping type*, the *payload*.

Ping types
==========

We send Telemetry with different ping types. The :doc:`main <../data/main-ping>` ping is the ping that contains the bulk of the Telemetry measurements for Firefox. For more specific use-cases, we send other ping types.

Pings sent from code that ships with Firefox are listed in the :doc:`data documentation <../data/index>`.

Important examples are:

* :doc:`main <../data/main-ping>` - contains the information collected by Telemetry (Histograms, hang stacks, ...)
* :doc:`saved-session <../data/main-ping>` - has the same format as a main ping, but it contains the *"classic"* Telemetry payload with measurements covering the whole browser session. This is only a separate type to make storage of saved-session easier server-side. This is temporary and will be removed soon.
* :doc:`crash <../data/crash-ping>` - a ping that is captured and sent after Firefox crashes.
* :doc:`new-profile <../data/new-profile-ping>` - sent on the first run of a new profile.
* :doc:`update <../data/update-ping>` - sent right after an update is downloaded.
* :doc:`deletion <../data/deletion-ping>` - sent when FHR upload is disabled, requesting deletion of the data associated with this user.
