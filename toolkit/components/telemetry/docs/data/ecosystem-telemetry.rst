Ecosystem Telemetry
===================

This module transmits Ecosystem Telemetry from Firefox Desktop.
It consists of two ping types: "pre-account" and "post-account".

.. note::

   The "pre-account" ping is not yet fully implemented and lacks some data to be included. Don't rely on any submitted data yet!
   The "post-account" ping is not yet implemented.
   See `bug 1522664 <https://bugzilla.mozilla.org/show_bug.cgi?id=1522664>`_ for pending work.

The client id is **not** submitted with either ping.
A reduced Telemetry environment is submitted in these pings.

Environment
-----------

This is a subset of the :doc:`environment`, due to privacy concerns.
Similar dimensions will be available on other products.

.. code-block:: js

    {
      settings: {
        locale: <string>, // e.g. "it", null on failure
      },
      system: {
        memoryMB: <number>,
        os: {
            name: <string>, // e.g. "Windows_NT", null on failure
            version: <string>, // e.g. "6.1", null on failure
            kernelVersion: <string>, // android only or null on failure
            servicePackMajor: <number>, // windows only or null on failure
            servicePackMinor: <number>, // windows only or null on failure
            windowsBuildNumber: <number>, // windows only or null on failure
            windowsUBR: <number>, // windows 10 only or null on failure
            installYear: <number>, // windows only or null on failure
            locale: <string>, // "en" or null on failure
        },
        cpu: {
          speedMHz: <number>, // cpu clock speed in MHz
        }
      },
      profile: {
        creationDate: <integer>, // integer days since UNIX epoch, e.g. 16446
        firstUseDate: <integer>, // integer days since UNIX epoch, e.g. 16446 - optional
      }
    }

Pre-account ping
----------------

.. code-block:: js

    {
      "type": "pre-account",
      ... common ping data
      "environment": { ... }, // as above
      "payload": {
        "reason": <string>, // Why the ping was submitted
        "ecosystemClientId": <string>, // Specific ecosystem client ID, not associated with regular client ID
        "uid": <string>, // Hashed account ID, for pings with reason "login" only
        "duration": <number>, // duration since ping was last sent or since the beginning of the Firefox session in seconds
        "histograms": {...},
        "keyedHistograms": {...},
        "scalars": {...},
        "keyedScalars": {...},
      }
    }

.. note::

   This ping does not yet submit a valid ``uid`` or ``ecosystemClientId``.
   See `bug 1530655 <https://bugzilla.mozilla.org/show_bug.cgi?id=1530655>`_.

reason
~~~~~~
The ``reason`` field contains the information about why the "pre-account" ping was submitted:

* ``periodic`` - Sent roughly every 24 hours
* ``shutdown`` - Sent on shutdown
* ``login`` - Sent when a user logs in
* ``logout`` - Sent when the user logs out

histograms and keyedHistograms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This section contains the :doc:`../collection/histograms` that are valid for the pre-account ping, per process.
The recorded histograms are described in `Histograms.json <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Histograms.json>`_, marked with the `pre-account` store.

scalars and keyedScalars
~~~~~~~~~~~~~~~~~~~~~~~~
This section contains the :doc:`../collection/scalars` that are valid for the pre-account ping, per process.
Scalars are only submitted if data was added to them.
The recorded scalars are described in `Scalars.yaml <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yml>`_, marked with the `pre-account` store.

Send behavior
-------------

Without an account
~~~~~~~~~~~~~~~~~~

A *pre-account* ping is submitted.
This ping is submitted roughly every 24 hours with reason *periodic*.
On shutdown this ping is submitted with reason *shutdown*.

When a user logs into Firefox Accounts, this ping is submitted with reason *login*.
If the user logs out and disconnects the account, this ping is submitted with reason *logout*.

With an account
~~~~~~~~~~~~~~~

.. note::

   Not yet implemented. See `Bug 1530654 <https://bugzilla.mozilla.org/show_bug.cgi?id=1530654>`_.

A *post-account* ping is submitted.
This ping is submitted roughly every 24 hours with reason *periodic*.
On shutdown this ping is submitted with reason *shutdown*.
