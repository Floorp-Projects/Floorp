
"main" ping
===========

This is the "main" Telemetry ping type, whose payload contains most of the measurements that are used to track the performance and health of Firefox in the wild.
It includes the histograms and other performance and diagnostic data.

This ping is triggered by different scenarios, which is documented by the ``reason`` field:

* ``environment-change`` - the :doc:`environment` changed, so the session measurements got reset and a new subsession starts
* ``shutdown`` - triggered when the browser session ends
* ``daily`` - a session split triggered in 24h hour intervals at local midnight
* ``saved-session`` - the *"classic"* Telemetry payload with measurements covering the whole browser session (only submitted for a transition period)

Most reasons lead to a session split, initiating a new *subsession*. We reset important measurements for those subsessions.

Structure::

    {
      version: 4,

      info: {
        reason: <string>, // what triggered this ping: "saved-session", "environment-change", "shutdown", ...
        revision: <string>, // the Histograms.json revision
        timezoneOffset: <number>, // time-zone offset from UTC, in minutes, for the current locale
        previousBuildId: <string>,

        sessionId: <uuid>,  // random session id, shared by subsessions
        subsessionId: <uuid>,  // random subsession id
        previousSubsessionId: <uuid>, // subsession id of the previous subsession (even if it was in a different session),
                                      // null on first run.

        subsessionCounter: <number>, // the running no. of this subsession since the start of the browser session
        profileSubsessionCounter: <number>, // the running no. of all subsessions for the whole profile life time

        sessionStartDate: <ISO date>, // daily precision
        subsessionStartDate: <ISO date>, // daily precision
        subsessionLength: <number>, // the subsession length in seconds
      },

      childPayloads: {...}, // only present with e10s; a reduced payload from content processes

      simpleMeasurements: { ... },
      histograms: {},
      keyedHistograms: {},
      chromeHangs: {},
      threadHangStats: {},
      log: [],
      fileIOReports: {...},
      lateWrites: {...},
      addonDetails: { ... },
      addonHistograms: {...},
      UIMeasurements: {...},
      slowSQL: {...},
      slowSQLstartup: {...},
    }
