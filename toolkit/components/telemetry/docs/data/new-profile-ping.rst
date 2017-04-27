
"new-profile" ping
==================

This opt-out ping is sent from Firefox Desktop 30 minutes after the browser is started, on the first session
of a newly created profile. If the first session of a newly-created profile was shorter than 30 minutes, it
gets sent using the :doc:`../internals/pingsender` at shutdown.

.. note::

  We don't sent the ping immediately after Telemetry completes initialization to give the user enough
  time to tweak their data collection preferences.

Structure:

.. code-block:: js

    {
      type: "new-profile",
      ... common ping data
      clientId: <UUID>,
      environment: { ... },
      payload: {
        reason: "startup" // or "shutdown"
      }
    }

payload.reason
--------------
If this field contains ``startup``, then the ping was generated at the scheduled time after
startup. If it contains ``shutdown``, then the browser was closed before the time the
ping was scheduled. In the latter case, the ping is generated during shutdown and sent
using the :doc:`../internals/pingsender`.

Duplicate pings
---------------
We expect a low fraction of duplicates of this ping, mostly due to crashes happening
right after sending the ping and before the telemetry state gets flushed to the disk. This should
be fairly low in practice and manageable during the analysis phase.
