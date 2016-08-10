
"crash" ping
============

This ping is captured after the main Firefox process crashes, whether or not the crash report is submitted to crash-stats.mozilla.org. It includes non-identifying metadata about the crash.

The environment block that is sent with this ping varies: if Firefox was running long enough to record the environment block before the crash, then the environment at the time of the crash will be recorded and ``hasCrashEnvironment`` will be true. If Firefox crashed before the environment was recorded, ``hasCrashEnvironment`` will be false and the recorded environment will be the environment at time of submission.

The client ID is submitted with this ping.

Structure:

.. code-block:: js

    {
      version: 1,
      type: "crash",
      ... common ping data
      clientId: <UUID>,
      environment: { ... },
      payload: {
        crashDate: "YYYY-MM-DD",
        sessionId: <UUID>, // may be missing for crashes that happen early
                           // in startup. Added in Firefox 48 with the
                           // intention of uplifting to Firefox 46
        metadata: {...}, // Annotations saved while Firefox was running. See nsExceptionHandler.cpp for more information
        hasCrashEnvironment: bool
      }
    }
