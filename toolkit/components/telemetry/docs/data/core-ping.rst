
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

Structure:

.. code-block:: js

    {
      "v": 9, // ping format version
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
      "distributionId": <string>, // Distribution identifier (optional)
      "created": <string>, // date the ping was created
                           // in local time, "yyyy-mm-dd"
      "tz": <integer>, // timezone offset (in minutes) of the
                       // device when the ping was created
      "sessions": <integer>, // number of sessions since last upload
      "durations": <integer>, // combined duration, in seconds, of all
                                    // sessions since last upload
      "searches": <object>, // Optional, object of search use counts in the
                            // format: { "engine.source": <pos integer> }
                            // e.g.: { "yahoo.suggestion": 3, "other.listitem": 1 }
      "experiments": [<string>, /* … */], // Optional, array of identifiers
                                    // for the active experiments
      "flashUsage": <integer>, // number of times flash plugin is played since last upload
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

distributionId
~~~~~~~~~~~~~~
The ``distributionId`` contains the distribution ID as specified by
preferences.json for a given distribution. More information on distributions
can be found `here <https://wiki.mozilla.org/Mobile/Distribution_Files>`_.

It is optional.

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

This field can also be ``null`` when a custom search engine is set as the
default.

sessions & durations
~~~~~~~~~~~~~~~~~~~~
On Android, a session is the time when Firefox is focused in the foreground.
`sessions` tracks the number of sessions since the last upload and
`durations` is the accumulated duration in seconds of all of these
sessions. Note that showing a dialog (including a Firefox dialog) will
take Firefox out of focus & end the current session.

An implementation that records a session when Firefox is completely hidden is
preferrable (e.g. to avoid the dialog issue above), however, it's more complex
to implement and so we chose not to, at least for the initial implementation.

profileDate
~~~~~~~~~~~
On Android, this value is created at profile creation time and retrieved or,
for legacy profiles, taken from the package install time (note: this is not the
same exact metric as profile creation time but we compromised in favor of ease
of implementation).

Additionally on Android, this field may be ``null`` in the unlikely event that
all of the following events occur:

#. The times.json file does not exist
#. The package install date could not be persisted to disk

The reason we don't just return the package install time even if the date could
not be persisted to disk is to ensure the value doesn't change once we start
sending it: we only want to send consistent values.

searches
~~~~~~~~
In the case a search engine is added by a user, the engine identifier "other" is used, e.g. "other.<source>".

Sources in Android are based on the existing UI telemetry values and are as
follows:

* actionbar: the user types in the url bar and hits enter to use the default
  search engine
* listitem: the user selects a search engine from the list of secondary search
  engines at the bottom of the screen
* suggestion: the user clicks on a search suggestion or, in the case that
  suggestions are disabled, the row corresponding with the main engine

Other parameters
----------------

HTTP "Date" header
~~~~~~~~~~~~~~~~~~
This header is used to track the submission date of the core ping in the format
specified by
`rfc 2616 sec 14.18 <https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.18>`_,
et al (e.g. "Tue, 01 Feb 2011 14:00:00 GMT").


Version history
---------------
* v9: changed ``arch`` to contain device arch rather than the one we built against
* v8: added ``flashUsage``
* v7: added ``sessionCount`` & ``sessionDuration``
* v6: added ``searches``
* v5: added ``created`` & ``tz``
* v4: ``profileDate`` will return package install time when times.json is not available
* v3: added ``defaultSearch``
* v2: added ``distributionId``
* v1: initial version - shipped in `Fennec 45 <https://bugzilla.mozilla.org/show_bug.cgi?id=1205835>`_.

Notes
~~~~~

* ``distributionId`` (v2) actually landed after ``profileDate`` (v4) but was
  uplifted to 46, whereas ``profileDate`` landed on 47. The version numbers in
  code were updated to be increasing (bug 1264492) and the version history docs
  rearranged accordingly.

Android implementation notes
----------------------------
On Android, the uploader has a high probability of delivering the complete data
for a given client but not a 100% probability. This was a conscious decision to
keep the code simple. The cases where we can lose data:

* Resetting the field measurements (including incrementing the sequence number)
  and storing a ping for upload are not atomic. Android can kill our process
  for memory pressure in between these distinct operations so we can just lose
  a ping's worth of data. That sequence number will be missing on the server.
* If we exceed some number of pings on disk that have not yet been uploaded,
  we remove old pings to save storage space. For those pings, we will lose
  their data and their sequence numbers will be missing on the server.

Note: we never expect to drop data without also dropping a sequence number so
we are able to determine when data loss occurs.
