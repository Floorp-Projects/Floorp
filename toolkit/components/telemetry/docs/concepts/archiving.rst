=========
Archiving
=========

When archiving is enabled through the relevant pref (``toolkit.telemetry.archive.enabled``), pings submitted to ``TelemetryController`` are also stored locally in the user profile directory, in ``<profile-dir>/datareporting/archived``.

To allow for cheaper lookup of archived pings, storage follows a specific naming scheme for both the directory and the ping file name: `<YYYY-MM>/<timestamp>.<UUID>.<type>.jsonlz4`.

* ``<YYYY-MM>`` - The subdirectory name, generated from the ping creation date.
* ``<timestamp>`` - Timestamp of the ping creation date.
* ``<UUID>`` - The ping identifier.
* ``<type>`` - The ping type.
