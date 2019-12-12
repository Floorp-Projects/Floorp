
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
      "v": 10, // ping format version
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
      "displayVersion": <string>, // Version displayed to user, e.g. 57.0b3 (optional)
      "distributionId": <string>, // Distribution identifier (optional)
      "campaignId": <string>, // Adjust's campaign identifier (optional)
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
      "accessibilityServices": [<string>, /* … */], // Optional, array of identifiers for
                                                    // enabled accessibility services that
                                                    // interact with our android products.
      "flashUsage": <integer>, // number of times flash plugin is played since last upload
      "defaultBrowser": <boolean> // true if the user has set Firefox as default browser
      "bug_1501329_affected": <boolean>  // true if Firefox previously used canary clientId
                                         // when submitting telemetry
      "fennec": <object> // Fennec only.
                         // Block of a variety of fields of different types.
                         // Used to understand the usage of Fennec features in the release population
                         // to understand when Fenix is ready to support Fennec users.
      {
        "new_tab": {
          "top_sites_clicked": <int>, // Number of times a Top Site was opened from the Awesome Screen.
                                      // Resets after each sent core ping.
          "pocket_stories_clicked": <int>, // Number of time a Pocket Recommended website was opened
                                           // from the Awesome Screen.
                                           // Resets after each sent core ping.
        }
        "settings_advanced": {
          "restore_tabs": <boolean>, // State of the "Settings/Advanced/Restore Tabs" setting
          "show_images": <string>, // State of the "Settings/Advanced/Show images" setting
                                   // Value will be be "user-specified" for any non-default values
          "show_web_fonts": <boolean>,  // State of the "Settings/Advanced/Show web fonts" setting
        },
        "settings_general": {
          "full_screen_browsing": <boolean>, // Current state of the
                                             // "Settings/General/Full-screen browsing" setting.
          "tab_queue": <boolean>, // State of the "Settings/General/Tab queue" setting.
          "tab_queue_usage_count": <int>, // Number of tabs opened through Tab Queue.
                                          // Resets after each sent core ping.
          "compact_tabs": <boolean>, // State of the "Settings/General/Compact tabs" setting.
          "homepage": {
            "custom_homepage": <boolean>, // "true" if not "about:home".
            "custom_homepage_use_for_newtab": <boolean>, // If the "Settings/General/Home/Also use for new tabs"
                                                         // setting is enabled.
            "topsites_enabled": <boolean>, // If the "Settings/General/Home/Top Sites"
                                           // setting is set to "Show".
            "pocket_enabled": <boolean>, // If the "Settings/General/Home/Top Sites/Recommended by Pocket"
                                         // setting is enabled.
            "recent_bookmarks_enabled": <boolean>, // If the "Settings/General/Home/Top Sites/
                                                   //          Additional Content/Recent Bookmarks"
                                                   // setting is enabled.
            "visited_enabled": <boolean>, // If the "Settings/General/Home/Top Sites/Additional Content/Visited"
                                          // setting is enabled.
            bookmarks_enabled": <boolean>, // If the "Settings/General/Home/Bookmarks" setting is set to "Show".
            "history_enabled": <boolean>, // If the "Settings/General/Home/History" setting is set to "Show".
          }
        },
        "settings_privacy": {
          "do_not_track": <boolean>, // If the "Settings/Privacy/Do not track" is enabled.
          "master_password": <boolean>, // If the "Settings/Privacy/Use master password" is enabled.
          "master_password_usage_count": <int>, // Number of times the user has entered their master password.
                                                // Resets after each sent core ping.
        },
        "settings_notifications": {
          "product_feature_tips": <boolean>, // If the "Settings/Notifications/Product and feature tips"
                                             // setting is enabled.
        },
        "addons": {
          "active": [addon_id_1, addon_id_2, …, ], // From all installed addons, which ones are active.
          "disabled": [addon_id_1, addon_id_2, …], // From all installed addons, which ones are disabled.
        },
        "page_options": {
          "save_as_pdf": <int>, // Number of times the user has used "Page/Save to PDF".
                                // Resets after each sent core ping.
          "print": <int>, // Number of times the user has used the "Page/Print".
                          // Resets after each sent core ping.
          "total_added_search_engines": <int>, // The absolute number of user added search engines,
                                               // not just those added during this session.
          "total_sites_pinned_to_topsites": <int>, // The absolute number of times the user has used
                                                   // the "Pin Site" functionality.
                                                   // Not just those added during this session.
          "view_source": <int>, // Number of times the user has used the "Page/View Page Source".
                                // Resets after each sent core ping.
          "bookmark_with_star": <int>, // The number of times the user has used the "Menu / <Star>".
                                       // Resets after each sent core ping.
          "current_pwas_count": <int>, // On Android >=25 - a positive number of PWAs currently on
                                       // homescreen, installed from this app.
                                       // On Android <25 - a default of "-1".
        },
        "sync": {
          "only_over_wifi": <boolean>, // "true" if the "Settings/Sync/Sync only over Wi-Fi"
                                       // setting is enabled.
                                       // null if the user is not signed into Sync.
        }
      }
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

campaignId
~~~~~~~~~~~~~~
The ``campaignId`` contains the campaign identifier like '3ly8t0'.
It's generated by `Adjust <https://docs.adjust.com/en/tracker-generation/#segmenting-users-dynamically-with-campaign-structure-parameters>`_,
It can only used to identify a campaign, but can't target to a specific user.

It is optional because not everyone has a campaign to begin with.

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
preferable (e.g. to avoid the dialog issue above), however, it's more complex
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
This describes the search engine usage(count). The format is { "<engine identifier>.<source>"" : count }
This is optional because the users may have never used the search feature.
There's no difference if extended telemetry is enabled (prerelease builds) or not.

Possible value :

.. code-block:: js

    {
       "yahoo.listitem":2,
       "duckduckgo.listitem":1,
       "google.suggestion":1
    }

**<engine identifier>**: the identifier of the the search engine. The identifier is collected the way same as desktop.
we only record the search engine name when:

* builtin or suggested search engines with an ID (includes partner search engines in various distribution scenarios).
  If it's not a built-in engine, we show "null" or "other".
* If the user has "Health Report" and core ping enabled.

**<sources>**: it's from one of the 'method's in UI telemetry. Possible values:

* actionbar: the user types in the url bar and hits enter to use the default
  search engine
* listitem: the user selects a search engine from the list of secondary search
  engines at the bottom of the screen
* suggestion: the user clicks on a search suggestion or, in the case that
  suggestions are disabled, the row corresponding with the main engine

accessibilityServices
~~~~~~~~~~~~~~~~~~~~~
This describes which accessibility services are currently enabled on user's device and could be interacting with our
products. This is optional because users often do not have any accessibility services enabled. If present, the value is
a list of accessibility service ids.

fennec.new_tab.top_sites_clicked
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `top_sites_clicked` field contains the number of times a top site was
opened from the new tab page since the last time the core ping was sent.
This counter is reset when the core ping has been sent.


Fennec.new_tab.pocket_stories_clicked
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `pocket_stories_clicked` contains the number of times a pocket story was
opened from the new tab page since the last time the core ping was sent.
This counter is reset when the core ping has been sent.

Fennec.settings_advanced.restore_tabs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `restore_tabs` field contains state of the "Settings/Advanced/Restore Tabs"
setting. It is true for "Always Restore" and false for "Don’t restore after
quitting Firefox".
The value is determined at the time of sending the core ping.

Fennec.settings_advanced.show_images
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `show_images` field contains the state of the
"Settings/Advanced/Show images" settings.
It is a string value set to "default" if the setting is "Always", or
"user~specified" for any of the other options.
The value is determined at the time of sending the core ping.

Fennec.settings_advanced.show_web_fonts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `show_web_fonts` field is a boolean that contains the current state of the
"Settings/Advanced/Show web fonts" setting.
The value is determined at the time of sending the core ping.

Fennec.settings_general.full_screen_browsing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `full_screen_browsing` field is a boolean that contains the current state
of the "Settings/General/Full~screen browsing" setting.
The value is determined at the time of sending the core ping.

Fennec.settings_general.tab_queue
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `tab_queue` field is a boolean that contains the current state of the
"Settings/General/Tab queue" setting.
The value is determined at the time of sending the core ping.

Fennec.settings_general.tab_queue_usage_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `tab_queue_usage_count` is a counter that increments with the number of
tabs opened through the tab queue.
It contains the total number of queued tabs opened since the last time the
Core Ping was sent.
This counter is reset when the core ping has been sent.

Fennec.settings_general.compact_tabs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `compact_tabs` field is a boolean that contains the current state of the
"Settings/General/Compact tabs" setting.
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.custom_homepage
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `custom_homepage` field is set to true if the homepage is not set to the
the default `about:home`.
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.custom_homepage_use_for_newtab
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `custom_homepage_use_for_newtab` field is set to true if the
"Settings/General/Home/Also use for new tabs" setting is enabled.
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.topsites_enabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `topsites_enabled` setting is true if the "Settings/General/Home/Top Sites"
setting is set to "Show".
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.pocket_enabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `pocket_enabled` setting is true if the
"Settings/General/Home/Top Sites/Recommended by Pocket" setting is enabled.
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.recent_bookmarks_enabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `recent_bookmarks_enabled` setting is true if the
"Settings/General/Home/Top Sites/Additional Content/Recent Bookmarks" setting
is enabled.
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.visited_enabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `visited_enabled` setting is true if the
"Settings/General/Home/Top Sites/Additional Content/Visited" setting is
enabled.
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.bookmarks_enabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `bookmarks_enabled` setting is true if the
"Settings/General/Home/Bookmarks" setting is set to "Show".
The value is determined at the time of sending the core ping.

Fennec.settings_general.homepage.history_enabled
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `history_enabled` setting is true if the "Settings/General/Home/History"
setting is set to "Show".
The value is determined at the time of sending the core ping.

Fennec.settings_privacy.do_not_track
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `do_not_track` setting is true if the "Settings/Privacy/Do not track" is
enabled.
The value is determined at the time of sending the core ping.

Fennec.settings_privacy.master_password
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `master_password` setting is true if the
"Settings/Privacy/Use master password" is enabled.
The value is determined at the time of sending the core ping.

Fennec.settings_privacy.master_password_usage_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `master_password_usage_count` field contains the number of times the user
has entered their master password since the last time the core ping was sent.
This counter is reset when the core ping has been sent.

Fennec.settings_notifications.product_feature_tips
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `product_feature_tips` setting is true if the
"Settings/Notifications/Product and feature tips" setting is enabled.
The value is determined at the time of sending the core ping.

fennec.page_options.save_as_pdf
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `save_as_pdf` field contains the number of times the user has used the
"Page/Save to PDF" feature since the last time the core ping was sent.
This counter is reset when the core ping has been sent.

fennec.page_options.print
~~~~~~~~~~~~~~~~~~~~~~~~~
The `print` field contains the number of times the user has used the
"Page/Print" feature since the last time the core ping was sent.
This counter is reset when the core ping has been sent.

fennec.page_options.total_added_search_engines
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `total_added_search_engines` is an absolute value that contains the number
of search engines the user has added manually.
The value is determined at the time of sending the core ping and never reset
to zero.

fennec.page_options.total_sites_pinned_to_topsites
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `total_sites_pinned_to_topsites` is an absolute value that contains the
number of sites the user has pinned to top sites.
The value is determined at the time of sending the core ping and never reset
to zero.

fennec.page_options.view_source
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `view_source` field contains the number of times the user has used the
"Page/View Page Source" feature since the last time the core ping was sent.
This counter is reset when the core ping has been sent.

Fennec.page_options.bookmark_with_star
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `bookmark_with_star` field contains the number of times the user has used
the "Menu / <Star>"" feature since the last time the core ping was sent.
This counter is reset when the core ping has been sent.

Fennec.page_options.current_pwas_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `current_pwas_count` field contains the number of currently installed PWAs
from this application.
As Android APIs for querying this are only available on Android >=25 for lower
versions of Android the value of this key will be "-1".
The value is determined at the time of sending the core ping.

Fennec.sync.only_over_wifi
~~~~~~~~~~~~~~~~~~~~~~~~~~
The `only_over_wifi` setting is true if the
"Settings/Sync/Sync only over Wi~Fi" setting is enabled.
The value is determined at the time of sending the core ping.
If the user is not signed into sync, then this value is set to `null`.
The value is determined at the time of sending the core ping.

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
* v10: added ``bug_1501329_affected``
* v9:

  - Apr 2017: changed ``arch`` to contain device arch rather than the one we
    built against & ``accessibilityServices``
  - Dec 2017: added ``defaultBrowser`` to know if the user has set Firefox as
    default browser (Dec 2017)
  - May 2018: added (optional) ``displayVersion`` to distinguish Firefox beta versions easily

* v8: added ``flashUsage``
* v7: added ``sessionCount`` & ``sessionDuration``  & ``campaignId``
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
