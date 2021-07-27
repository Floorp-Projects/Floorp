Ecosystem Telemetry (obsolete)
==============================

This module transmits Ecosystem Telemetry from Firefox Desktop.
It is only sent for Firefox Account users, using a single ping type
"account-ecosystem"

.. note::

  You might like to read the `background information on Ecosystem
  Telemetry <https://mozilla.github.io/ecosystem-platform/docs/features/firefox-accounts/ecosystem-telemetry/>`_

The existing telemetry client id is **not** submitted with the ping, but an
"ecosystem client id" is - this has the same semantics as the existing
client id, although is a different value, and is not sent in any other ping.

An anonymized user ID is submitted with each ping - `read more about these
IDs and how they're designed to safeguard user privacy <https://mozilla.github.io/ecosystem-platform/docs/features/firefox-accounts/ecosystem-telemetry/>`_

A reduced Telemetry environment is submitted in the ping, as described below.

Environment
-----------

In an effort to reduce the possibility of fingerprinting, we only provide the
following environment subset:

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

account-ecosystem ping
----------------------

.. code-block:: js

    {
      "type": "account-ecosystem",
      ... common ping data
      "environment": { ... }, // as above
      "payload": {
        "reason": <string>, // Why the ping was submitted
        "ecosystemAnonId": <string>, // The anonymized ID, as described above.
        "ecosystemClientId": <guid>, // The ecosystem client ID as described above.
        "duration": <number>, // duration since ping was last sent or since the beginning of the Firefox session in seconds
        "histograms": {...},
        "keyedHistograms": {...},
        "scalars": {...},
        "keyedScalars": {...},
      }
    }

reason
~~~~~~
The ``reason`` field contains the information about why the "account-ecosystem" ping was submitted:

* ``periodic`` - Sent roughly every 24 hours
* ``shutdown`` - Sent on shutdown
* ``logout`` - Sent when the user logs out

histograms and keyedHistograms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This section contains the :doc:`../collection/histograms` that are valid for the account-ecosystem ping, per process.
The recorded histograms are described in `Histograms.json <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Histograms.json>`_, marked with the `account-ecosystem` store.

scalars and keyedScalars
~~~~~~~~~~~~~~~~~~~~~~~~
This section contains the :doc:`../collection/scalars` that are valid for the account-ecosystem ping, per process.
Scalars are only submitted if data was added to them.
The recorded scalars are described in `Scalars.yaml <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Scalars.yaml>`_, marked with the `account-ecosystem` store.

Send behavior
-------------

Without an account
~~~~~~~~~~~~~~~~~~

Never.

When a user logs into Firefox Accounts, this ping is submitted as described in
"With an account" below. No ping is immediately sent.

With an account
~~~~~~~~~~~~~~~

The ping is submitted; roughly every 24 hours with reason *periodic*. On
shutdown this ping is submitted with reason *shutdown*.

If the user logs out and disconnects the account, this ping is submitted with
reason *logout*. While logged out, no pings will be submitted.
