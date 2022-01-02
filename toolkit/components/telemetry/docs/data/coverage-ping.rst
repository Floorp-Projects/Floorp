
"coverage" ping
===============

This ping is not enabled by default. When enabled, a ping is generated a total of once per profile, as a diagnostic tool
to determine whether Telemetry is working for users.

This ping contains no client id and no environment data.

You can find more background information in `this blog post <https://blog.mozilla.org/data/2018/08/20/effectively-measuring-search-in-firefox/>`_.

Structure:

.. code-block:: js

    {
      "appVersion": "63.0a1",
      "appUpdateChannel": "nightly",
      "osName": "Darwin",
      "osVersion": "17.7.0",
      "telemetryEnabled": 1
    }

Expected behaviours
-------------------
The following is a list of expected behaviours for the ``coverage`` ping:

- The ping will only be sent once per ping version, per profile.
- If sending the ping fails, it will be retried on startup.
- A totally arbitrary UUID is generated on first run on a new profile, to use for filtering duplicates.
- The ping is sent to a different endpoint not using existing Telemetry.
- The ping does not honor the Telemetry enabled preference, but provides its own opt-out preference: `toolkit.coverage.opt-out`.
- The ping is disabled by default. It is intended to be enabled for users on an experimental basis using the preference `toolkit.coverage.enabled`.

Version History
---------------

- Firefox 64:

  - "coverage" ping shipped (`bug 1492656 <https://bugzilla.mozilla.org/show_bug.cgi?id=1492656>`_).
