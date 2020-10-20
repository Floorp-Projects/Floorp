
"uninstall" ping
================

This opt-out ping is sent from the Windows uninstaller when the uninstall finishes. Notably it includes ``clientId`` and the :doc:`Telemetry Environment <environment>`. It follows the :doc:`common ping format <common-ping>`.

Structure:

.. code-block:: js

    {
      type: "uninstall",
      ... common ping data
      clientId: <UUID>,
      environment: { ... },
      payload: {
        otherInstalls: <integer>, // Optional, number of other installs on the system, max 11.
      }
    }

See also the `JSON schema <https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/templates/telemetry/uninstall/uninstall.4.schema.json>`_. These pings are recorded in the ``telemetry.uninstall`` table in Redash, using the default "Telemetry (BigQuery)" data source.

payload.otherInstalls
---------------------
This is a count of how many other installs of Firefox were present on the system at the time the ping was written. It is the number of values in the ``Software\Mozilla\Firefox\TaskBarIDs`` registry key, for both 32-bit and 64-bit architectures, for both HKCU and HKLM, excluding duplicates, and excluding a value for this install (if present). For example, if this is the only install on the system, the value will be 0. It may be missing in case of an error.

This count is capped at 11. This avoids introducing a high-resolution identifier in case of a system with a large, unique number of installs.

Uninstall Ping storage and lifetime
-----------------------------------

On delayed Telemetry init (about 1 minute into each run of Firefox), if opt-out telemetry is enabled, this ping is written to disk. There is a single ping for each install, any uninstall pings from the same install are removed before the new ping is written.

The ping is removed if Firefox notices that opt-out telemetry has been disabled, either when the ``datareporting.healthreport.uploadEnabled`` pref goes false or when it is false on delayed init. Conversely, when opt-out telemetry is re-enabled, the ping is written as Telemetry is setting itself up again.

The ping is sent by the uninstaller some arbitrary time after it is written to disk by Firefox, so it could be significantly out of date when it is submitted. There should be little impact from stale data, since analysis is likely to focus on clients that uninstalled soon after running Firefox, and this ping mostly changes when Firefox itself is updated.
