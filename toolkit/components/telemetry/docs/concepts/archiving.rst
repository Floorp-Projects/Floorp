=========
Archiving
=========

When archiving is enabled through the relevant pref (``toolkit.telemetry.archive.enabled``), pings submitted to ``TelemetryController`` are also stored locally in the user profile directory, in ``<profile-dir>/datareporting/archived``.

To allow for cheaper lookup of archived pings, storage follows a specific naming scheme for both the directory and the ping file name: `<YYYY-MM>/<timestamp>.<UUID>.<type>.jsonlz4`.

* ``<YYYY-MM>`` - The subdirectory name, generated from the ping creation date.
* ``<timestamp>`` - Timestamp of the ping creation date.
* ``<UUID>`` - The ping identifier.
* ``<type>`` - The ping type.

Archived data can be viewed on ``about:telemetry``.

Cleanup
-------

Archived pings are not kept around forever.
After startup of Firefox and initialization of Telemetry, the archive is cleaned up if necessary.

* Old ping data is removed by month if it is older than 60 days.
* If the total size of the archive exceeds the quota of 120 MB, pings are removed to reduce the size of the archive again.
