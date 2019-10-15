
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
        reason: "startup", // or "shutdown"
        processes: { ... }
      }
    }

payload.reason
--------------
If this field contains ``startup``, then the ping was generated at the scheduled time after
startup. If it contains ``shutdown``, then the browser was closed before the time the
ping was scheduled. In the latter case, the ping is generated during shutdown and sent
using the :doc:`../internals/pingsender`.

processes
---------
This section contains per-process data.

Structure:

.. code-block:: js

    "processes" : {
      "parent": {
        "scalars": {...}
      }
    }

scalars
~~~~~~~
This section contains the :doc:`../collection/scalars` that are valid for the ``new-profile`` ping,
that is the ``record_into_store`` list contains ``new-profile``.
Scalars are only submitted if data was added to them.
The recorded scalars are described in the `Scalars.yaml <https://dxr.mozilla.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yaml>`_ file.

Duplicate pings
---------------
We expect a low fraction of duplicates of this ping, mostly due to crashes happening
right after sending the ping and before the telemetry state gets flushed to the disk. This should
be fairly low in practice and manageable during the analysis phase.

Expected behaviours
-------------------
The following is a list of conditions and expected behaviours for the ``new-profile`` ping:

- **The ping is generated at the browser shutdown on a new profile, after the privacy policy is displayed:**

  - *for an user initiated browser shutdown*, ``new-profile`` is sent immediately using the :doc:`../internals/pingsender`;
  - *for a browser shutdown triggered by OS shutdown*, ``new-profile`` is saved to disk and sent next time the browser restarts.
- **The ping is generated before the privacy policy is displayed**: ``new-profile`` is saved to disk and sent
  next time the browser restarts.
- **The ping is set to be generated and Telemetry is disabled**: ``new-profile`` is never sent, even if Telemetry is
  turned back on later.
- **Firefox crashes before the ping can be generated**: ``new-profile`` will be scheduled to be generated and
  sent again on the next restart.
- **User performs a profile refresh**:

  - *the ping was already sent*: ``new-profile`` will not be sent again.
  - *the ping was not sent*: ``new-profile`` will be generated and sent.
  - *the refresh happens immediately after the profile creation, before the policy is shown*: ``new-profile`` will not be sent again.
- **Firefox is run with an old profile that already sent Telemetry data**: ``new-profile`` will not be generated
  nor sent.
