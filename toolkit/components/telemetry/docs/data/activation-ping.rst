
"activation" ping
==================

This mobile-specific ping is intendend to track activations of distributions of mobile products
with a small error rate.

This will be sent at Startup. Minimally, we want to get this ping at least once from every client.

Submission will be per the Edge server specification::

    /submit/mobile/activation/v/docId

This is a modern “structure ingestion” ping (the namespace is not telemetry). For structured
ingestion, we capture the schema version as one of the URI parameters, so we don’t need to
include it in the body of the message.

* ``v`` is the ping format version
* ``docId`` is a UUID for deduping

Structure:

.. code-block:: js

    {
      "identifier": <string>, // Googled Ad ID hashed using bcrypt
      "clientId": <string>, // client id, e.g. "c641eacf-c30c-4171-b403-f077724e848a"
                            //included only if identifier was unabled to be retrieved
      "manufacturer": <string>, // Build.MANUFACTURER
      "model": <string>, // Build.MODEL
      "locale": <string>, // application locale, e.g. "en-US"
      "os": <string>, // OS name.
      "osversion": <string>, // OS version.
      "created": <string>, // date the ping was created
                           // in local time, "yyyy-mm-dd"
      "tz": <integer>, // timezone offset (in minutes) of the
                       // device when the ping was created
      "app_name": <string>, // "Fennec"
      "channel": <string>, // Android package name e.g. "org.mozilla.firefox"
      "distributionId": <string> // Distribution identifier (optional)
    }


Field details
-------------

identifier
~~~~~~~~~~
The ``identifier`` field is the Google Ad ID hashed using bcrypt. Ideally we want to send this instead of the
client_id but not all distributions have Google Play Services enabled.

client_id
~~~~~~~~~~
The ``client_id`` field represents the telemetry client id and it is only included if the identifier is empty.

channel
~~~~~~~
The ``channel`` field represents the Android package name.

Version history
---------------
* v1: initial version - shipped in `Fennec 68 <https://bugzilla.mozilla.org/show_bug.cgi?id=1534451>`_.

Android implementation notes
----------------------------
On Android, the uploader has a high probability of delivering the complete data
for a given client but not a 100% probability. This was a conscious decision to
keep the code simple. Even if we drop a ping, it will be resent on future startups
until we have confirmation that it has been uploaded.

