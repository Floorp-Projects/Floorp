
"core" ping
============

This mobile-specific ping is intended to provide the most critical
data in a concise format, allowing for frequent uploads.

Since this ping is used to measure retention, it should be sent
each time the browser is opened.

Submission will be per the Edge server specification::

    /submit/telemetry/docId/docType/appName/appVersion/appUpdateChannel/appBuildID

* ``docId`` is a UUID for deduping
* ``docType`` is “core”
* ``appName`` is “Fennec”
* ``appVersion`` is the version of the application (e.g. "46.0a1")
* ``appUpdateChannel`` is “release”, “beta”, etc.
* ``appBuildID`` is the build number

Note: Counts below (e.g. search & usage times) are “since the last
ping”, not total for the whole application lifetime.

Structure::

    {
      "v": 2, // ping format version
      "clientId": <string>, // client id, e.g.
                            // "c641eacf-c30c-4171-b403-f077724e848a"
      "seq": <positive integer>, // running ping counter, e.g. 3
      "locale": <string>, // application locale, e.g. "en-US"
      "os": <string>, // OS name.
      "osversion": <string>, // OS version.
      "device": <string>, // Build.MANUFACTURER + " - " + Build.MODEL
                          // where manufacturer is truncated to 12 characters
                          // & model is truncated to 19 characters
      "arch": <string>, // e.g. "arm", "x86"
      "profileDate": <pos integer>, // Profile creation date in days since
                                    // UNIX epoch.
      "defaultSearch": <string>, // Identifier of the default search engine,
                                 // e.g. "yahoo".

      "experiments": [<string>, …], // Optional, array of identifiers
                                    // for the active experiments
    }

Field details
-------------

device
~~~~~~
The ``device`` field is filled in with information specified by the hardware
manufacturer. As such, it could be excessively long and use excessive amounts
of limited user data. To avoid this, we limit the length of the field. We're
more likely have collisions for models within a manufacturer (e.g. "Galaxy S5"
vs. "Galaxy Note") than we are for shortened manufacturer names so we provide
more characters for the model than the manufacturer.

defaultSearch
~~~~~~~~~~~~~
On Android, this field may be ``null``. To get the engine, we rely on
``SearchEngineManager#getDefaultEngine``, which searches in several places in
order to find the search engine identifier:

* Shared Preferences
* The distribution (if it exists)
* The localized default engine

If the identifier could not be retrieved, this field is ``null``. If the
identifier is retrieved, we attempt to create an instance of the search
engine from the search plugins (in order):

* In the distribution
* From the localized plugins shipped with the browser
* The third-party plugins that are installed in the profile directory

If the plugins fail to create a search engine instance, this field is also
``null``.

profileDate
~~~~~~~~~~~

This field may be missing if `times.json` does not exist or could not be read from the profile.
`Bug 1246816 <https://bugzilla.mozilla.org/show_bug.cgi?id=1246816>`_ will fix that with a fallback mechanism.

Version history
---------------
* v2: added ``defaultSearch``
* v1: initial version
