.. _healthreport_dataformat:

==============
Payload Format
==============

Currently, the Firefox Health Report is submitted as a compressed JSON
document. The root JSON element is an object. A *version* field defines
the version of the payload which in turn defines the expected contents
the object.

As of 2013-07-03, desktop submits Version 2, and Firefox for Android submits
Version 3 payloads.

Version 3
=========

Version 3 is a complete rebuild of the document format. Events are tracked in
an "environment". Environments are computed from a large swath of local data
(e.g., add-ons, CPU count, versions), and a new environment comes into being
when one of its attributes changes.

Client documents, then, will include descriptions of many environments, and
measurements will be attributed to one particular environment.

A map of environments is present at the top level of the document, with the
current named "current" in the map. Each environment has a hash identifier and
a set of attributes. The current environment is completely described, and has
its hash present in a "hash" attribute. All other environments are represented
as a tree diff from the current environment, with their hash as the key in the
"environments" object.

A removed add-on has the value 'null'.

There is no "last" data at present.

Daily data is hierarchical: by day, then by environment, and then by
measurement, and is present in "data", just as in v2.

Leading by example::

    {
      "lastPingDate": "2013-06-29",
      "thisPingDate": "2013-07-03",
      "version": 3,
      "environments": {
        "current": {
          "org.mozilla.sysinfo.sysinfo": {
            "memoryMB": 1567,
            "cpuCount": 4,
            "architecture": "armeabi-v7a",
            "_v": 1,
            "version": "4.1.2",
            "name": "Android"
          },
          "org.mozilla.profile.age": {
            "_v": 1,
            "profileCreation": 15827
          },
          "org.mozilla.addons.active": {
            "QuitNow@TWiGSoftware.com": {
              "appDisabled": false,
              "userDisabled": false,
              "scope": 1,
              "updateDay": 15885,
              "foreignInstall": false,
              "hasBinaryComponents": false,
              "blocklistState": 0,
              "type": "extension",
              "installDay": 15885,
              "version": "1.18.02"
            },
            "{dbbf9331-b713-6eda-1006-205efead09dc}": {
              "appDisabled": false,
              "userDisabled": "askToActivate",
              "scope": 8,
              "updateDay": 15779,
              "foreignInstall": true,
              "blocklistState": 0,
              "type": "plugin",
              "installDay": 15779,
              "version": "11.1 r115"
            },
            "desktopbydefault@bnicholson.mozilla.org": {
              "appDisabled": false,
              "userDisabled": true,
              "scope": 1,
              "updateDay": 15870,
              "foreignInstall": false,
              "hasBinaryComponents": false,
              "blocklistState": 0,
              "type": "extension",
              "installDay": 15870,
              "version": "1.1"
            },
            "{6e092a7f-ba58-4abb-88c1-1a4e50b217e4}": {
              "appDisabled": false,
              "userDisabled": false,
              "scope": 1,
              "updateDay": 15828,
              "foreignInstall": false,
              "hasBinaryComponents": false,
              "blocklistState": 0,
              "type": "extension",
              "installDay": 15828,
              "version": "1.1.0"
            },
            "{46551EC9-40F0-4e47-8E18-8E5CF550CFB8}": {
              "appDisabled": false,
              "userDisabled": true,
              "scope": 1,
              "updateDay": 15879,
              "foreignInstall": false,
              "hasBinaryComponents": false,
              "blocklistState": 0,
              "type": "extension",
              "installDay": 15879,
              "version": "1.3.2"
            },
            "_v": 1
          },
          "org.mozilla.appInfo.appinfo": {
            "_v": 3,
            "appLocale": "en_us",
            "osLocale": "en_us",
            "distribution": "",
            "acceptLangIsUserSet": 0,
            "isTelemetryEnabled": 1,
            "isBlocklistEnabled": 1
          },
          "geckoAppInfo": {
            "updateChannel": "nightly",
            "id": "{aa3c5121-dab2-40e2-81ca-7ea25febc110}",
            "os": "Android",
            "platformBuildID": "20130703031323",
            "platformVersion": "25.0a1",
            "vendor": "Mozilla",
            "name": "fennec",
            "xpcomabi": "arm-eabi-gcc3",
            "appBuildID": "20130703031323",
            "_v": 1,
            "version": "25.0a1"
          },
          "hash": "tB4Pnnep9yTxnMDymc3dAB2RRB0=",
          "org.mozilla.addons.counts": {
            "extension": 4,
            "plugin": 1,
            "_v": 1,
            "theme": 0
          }
        },
        "k2O3hlreMeS7L1qtxeMsYWxgWWQ=": {
          "geckoAppInfo": {
            "platformBuildID": "20130630031138",
            "appBuildID": "20130630031138",
            "_v": 1
          },
          "org.mozilla.appInfo.appinfo": {
            "_v": 2,
          }
        },
        "1+KN9TutMpzdl4TJEl+aCxK+xcw=": {
          "geckoAppInfo": {
            "platformBuildID": "20130626031100",
            "appBuildID": "20130626031100",
            "_v": 1
          },
          "org.mozilla.addons.active": {
            "QuitNow@TWiGSoftware.com": null,
            "{dbbf9331-b713-6eda-1006-205efead09dc}": null,
            "desktopbydefault@bnicholson.mozilla.org": null,
            "{6e092a7f-ba58-4abb-88c1-1a4e50b217e4}": null,
            "{46551EC9-40F0-4e47-8E18-8E5CF550CFB8}": null,
            "_v": 1
          },
          "org.mozilla.addons.counts": {
            "extension": 0,
            "plugin": 0,
            "_v": 1
          }
        }
      },
      "data": {
        "last": {},
        "days": {
          "2013-07-03": {
            "tB4Pnnep9yTxnMDymc3dAB2RRB0=": {
              "org.mozilla.appSessions": {
                "normal": [
                  {
                    "r": "P",
                    "d": 2,
                    "sj": 653
                  },
                  {
                    "r": "P",
                    "d": 22
                  },
                  {
                    "r": "P",
                    "d": 5
                  },
                  {
                    "r": "P",
                    "d": 0
                  },
                  {
                    "r": "P",
                    "sg": 3560,
                    "d": 171,
                    "sj": 518
                  },
                  {
                    "r": "P",
                    "d": 16
                  },
                  {
                    "r": "P",
                    "d": 1079
                  }
                ],
                "_v": "4"
              }
            },
            "k2O3hlreMeS7L1qtxeMsYWxgWWQ=": {
              "org.mozilla.appSessions": {
                "normal": [
                  {
                    "r": "P",
                    "d": 27
                  },
                  {
                    "r": "P",
                    "d": 19
                  },
                  {
                    "r": "P",
                    "d": 55
                  }
                ],
                "_v": "4"
              },
              "org.mozilla.searches.counts": {
                "bartext": {
                  "google": 1
                },
                "_v": "4"
              },
              "org.mozilla.experiment": {
                "lastActive": "some.experiment.id"
                "_v": "1"
              }
            }
          }
        }
      }
    }

App sessions in Version 3
-------------------------

Sessions are divided into "normal" and "abnormal". Session objects are stored as discrete JSON::

    "org.mozilla.appSessions": {
      _v: 4,
      "normal": [
        {"r":"P", "d": 123},
      ],
      "abnormal": [
        {"r":"A", "oom": true, "stopped": false}
      ]
    }

Keys are:

"r"
    reason. Values are "P" (activity paused), "A" (abnormal termination).
"d"
    duration. Value in seconds.
"sg"
    Gecko startup time (msec). Present if this is a clean launch. This
    corresponds to the telemetry timer *FENNEC_STARTUP_TIME_GECKOREADY*.
"sj"
    Java activity init time (msec). Present if this is a clean launch. This
    corresponds to the telemetry timer *FENNEC_STARTUP_TIME_JAVAUI*,
    and includes initialization tasks beyond initial
    *onWindowFocusChanged*.

Abnormal terminations will be missing a duration and will feature these keys:

"oom"
    was the session killed by an OOM exception?
"stopped"
    was the session stopped gently?

Version 3.1
-----------

As of Firefox 27, *appinfo* is now bumped to v3, including *osLocale*,
*appLocale* (currently always the same as *osLocale*), *distribution* (a string
containing the distribution ID and version, separated by a colon), and
*acceptLangIsUserSet*, an integer-boolean that describes whether the user set
an *intl.accept_languages* preference.

The search counts measurement is now at version 5, which indicates that
non-partner searches are recorded. You'll see identifiers like "other-Foo Bar"
rather than "other".

Other notable differences from Version 2
----------------------------------------

* There is no default browser indicator on Android.
* Add-ons include a *blocklistState* attribute, as returned by AddonManager.
* Searches are now version 4, and are hierarchical: how the search was started
  (bartext, barkeyword, barsuggest), and then counts per provider.

Version 2
=========

Version 2 is the same as version 1 with the exception that it has an additional
top-level field, *geckoAppInfo*, which contains basic application info.

geckoAppInfo
------------

This field is an object that is a simple map of string keys and values
describing basic application metadata. It is very similar to the *appinfo*
measurement in the *last* section. The difference is this field is almost
certainly guaranteed to exist whereas the one in the data part of the
payload may be omitted in certain scenarios (such as catastrophic client
error).

Its keys are as follows:

appBuildID
    The build ID/date of the application. e.g. "20130314113542".

version
    The value of nsXREAppData.version. This is the application's version. e.g.
    "21.0.0".

vendor
    The value of nsXREAppData.vendor. Can be empty an empty string. For
    official Mozilla builds, this will be "Mozilla".

name
    The value of nsXREAppData.name. For official Firefox builds, this
    will be "Firefox".

id
    The value of nsXREAppData.ID.

platformVersion
    The version of the Gecko platform (as opposed to the app version). For
    Firefox, this is almost certainly equivalent to the *version* field.

platformBuildID
    The build ID/date of the Gecko platfor (as opposed to the app version).
    This is commonly equivalent to *appBuildID*.

os
    The name of the operating system the application is running on.

xpcomabi
    The binary architecture of the build.

updateChannel
    The name of the channel used for application updates. Official Mozilla
    builds have one of the values {release, beta, aurora, nightly}. Local and
    test builds have *default* as the channel.

Version 1
=========

Top-level Properties
--------------------

The main JSON object contains the following properties:

lastPingDate
    UTC date of the last upload. If this is the first upload from this client,
    this will not be present.

thisPingDate
    UTC date when this payload was constructed.

version
    Integer version of this payload format. Currently only 1 is defined.

data
    Object holding data constituting health report.

Data Properties
---------------

The bulk of the health report is contained within the *data* object. This
object has the following keys:

days
   Object mapping UTC days to measurements from that day. Keys are in the
   *YYYY-MM-DD* format. e.g. "2013-03-14"

last
   Object mapping measurement names to their values.


The value of *days* and *last* are objects mapping measurement names to that
measurement's values. The values are always objects. Each object contains
a *_v* property. This property defines the version of this measurement.
Additional non-underscore-prefixed properties are defined by the measurement
itself (see sections below).

Example
-------

Here is an example JSON document for version 1::

    {
      "version": 1,
      "thisPingDate": "2013-03-11",
      "lastPingDate": "2013-03-10",
      "data": {
        "last": {
          "org.mozilla.addons.active": {
            "masspasswordreset@johnathan.nightingale": {
              "userDisabled": false,
              "appDisabled": false,
              "version": "1.05",
              "type": "extension",
              "scope": 1,
              "foreignInstall": false,
              "hasBinaryComponents": false,
              "installDay": 14973,
              "updateDay": 15317
            },
            "places-maintenance@bonardo.net": {
              "userDisabled": false,
              "appDisabled": false,
              "version": "1.3",
              "type": "extension",
              "scope": 1,
              "foreignInstall": false,
              "hasBinaryComponents": false,
              "installDay": 15268,
              "updateDay": 15379
            },
            "_v": 1
          },
          "org.mozilla.appInfo.appinfo": {
            "_v": 1,
            "appBuildID": "20130309030841",
            "distributionID": "",
            "distributionVersion": "",
            "hotfixVersion": "",
            "id": "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}",
            "locale": "en-US",
            "name": "Firefox",
            "os": "Darwin",
            "platformBuildID": "20130309030841",
            "platformVersion": "22.0a1",
            "updateChannel": "nightly",
            "vendor": "Mozilla",
            "version": "22.0a1",
            "xpcomabi": "x86_64-gcc3"
          },
          "org.mozilla.profile.age": {
            "_v": 1,
            "profileCreation": 12444
          },
          "org.mozilla.appSessions.current": {
            "_v": 3,
            "startDay": 15773,
            "activeTicks": 522,
            "totalTime": 70858,
            "main": 1245,
            "firstPaint": 2695,
            "sessionRestored": 3436
          },
          "org.mozilla.sysinfo.sysinfo": {
            "_v": 1,
            "cpuCount": 8,
            "memoryMB": 16384,
            "architecture": "x86-64",
            "name": "Darwin",
            "version": "12.2.1"
          }
        },
        "days": {
          "2013-03-11": {
            "org.mozilla.addons.counts": {
              "_v": 1,
              "extension": 15,
              "plugin": 12,
              "theme": 1
            },
            "org.mozilla.places.places": {
              "_v": 1,
              "bookmarks": 757,
              "pages": 104858
            },
            "org.mozilla.appInfo.appinfo": {
              "_v": 1,
              "isDefaultBrowser": 1
            }
          },
          "2013-03-10": {
            "org.mozilla.addons.counts": {
              "_v": 1,
              "extension": 15,
              "plugin": 12,
              "theme": 1
            },
            "org.mozilla.places.places": {
              "_v": 1,
              "bookmarks": 757,
              "pages": 104857
            },
            "org.mozilla.searches.counts": {
              "_v": 1,
              "google.urlbar": 4
            },
            "org.mozilla.appInfo.appinfo": {
              "_v": 1,
              "isDefaultBrowser": 1
            }
          }
        }
      }
    }

Measurements
============

The bulk of payloads consists of measurement data. An individual measurement
is merely a collection of related values e.g. *statistics about the Places
database* or *system information*.

Each measurement has an integer version number attached. When the fields in
a measurement or the semantics of data within that measurement change, the
version number is incremented.

All measurements are defined alphabetically in the sections below.

org.mozilla.addons.addons
-------------------------

This measurement contains information about the currently-installed add-ons.

Version 2
^^^^^^^^^

This version adds the human-readable fields *name* and *description*, both
coming directly from the Addon instance as most properties in version 1.
Also, all plugin details are now in org.mozilla.addons.plugins.

Version 1
^^^^^^^^^

The measurement object is a mapping of add-on IDs to objects containing
add-on metadata.

Each add-on contains the following properties:

* userDisabled
* appDisabled
* version
* type
* scope
* foreignInstall
* hasBinaryComponents
* installDay
* updateDay

With the exception of *installDay* and *updateDay*, all these properties
come direct from the Addon instance. See https://developer.mozilla.org/en-US/docs/Addons/Add-on_Manager/Addon.
*installDay* and *updateDay* are the number of days since UNIX epoch of
the add-ons *installDate* and *updateDate* properties, respectively.

Notes
^^^^^

Add-ons that have opted out of AMO updates via the
*extensions._id_.getAddons.cache.enabled* preference are, since Bug 868306
(Firefox 24), included in the list of submitted add-ons.

Example
^^^^^^^
::

    "org.mozilla.addons.addons": {
      "_v": 2,
      "{d10d0bf8-f5b5-c8b4-a8b2-2b9879e08c5d}": {
        "userDisabled": false,
        "appDisabled": false,
        "name": "Adblock Plus",
        "version": "2.4.1",
        "type": "extension",
        "scope": 1,
        "description": "Ads were yesterday!",
        "foreignInstall": false,
        "hasBinaryComponents": false,
        "installDay": 16093,
        "updateDay": 16093
      },
      "{e4a8a97b-f2ed-450b-b12d-ee082ba24781}": {
        "userDisabled": true,
        "appDisabled": false,
        "name": "Greasemonkey",
        "version": "1.14",
        "type": "extension",
        "scope": 1,
        "description": "A User Script Manager for Firefox",
        "foreignInstall": false,
        "hasBinaryComponents": false,
        "installDay": 16093,
        "updateDay": 16093
      }
    }

org.mozilla.addons.plugins
-------------------------

This measurement contains information about the currently-installed plugins.

Version 1
^^^^^^^^^

The measurement object is a mapping of plugin IDs to objects containing
plugin metadata.

The plugin ID is constructed of the plugins filename, name, version and
description. Every plugin has at least a filename and a name.

Each plugin contains the following properties:

* name
* version
* description
* blocklisted
* disabled
* clicktoplay
* mimeTypes
* updateDay

With the exception of *updateDay* and *mimeTypes*, all these properties come
directly from ``nsIPluginTag`` via ``nsIPluginHost``.
*updateDay* is the number of days since UNIX epoch of the plugins last modified
time.
*mimeTypes* is the list of mimetypes the plugin supports, see
``nsIPluginTag.getMimeTypes()`.

Example
^^^^^^^

::

    "org.mozilla.addons.plugins": {
      "_v": 1,
      "Flash Player.plugin:Shockwave Flash:12.0.0.38:Shockwave Flash 12.0 r0": {
        "mimeTypes": [
          "application/x-shockwave-flash",
          "application/futuresplash"
        ],
        "name": "Shockwave Flash",
        "version": "12.0.0.38",
        "description": "Shockwave Flash 12.0 r0",
        "blocklisted": false,
        "disabled": false,
        "clicktoplay": false
      },
      "Default Browser.plugin:Default Browser Helper:537:Provides information about the default web browser": {
        "mimeTypes": [
          "application/apple-default-browser"
        ],
        "name": "Default Browser Helper",
        "version": "537",
        "description": "Provides information about the default web browser",
        "blocklisted": false,
        "disabled": true,
        "clicktoplay": false
      }
    }

org.mozilla.addons.counts
-------------------------

This measurement contains information about historical add-on counts.

Version 1
^^^^^^^^^

The measurement object consists of counts of different add-on types. The
properties are:

extension
    Integer count of installed extensions.
plugin
    Integer count of installed plugins.
theme
    Integer count of installed themes.
lwtheme
    Integer count of installed lightweigh themes.

Notes
^^^^^

Add-ons opted out of AMO updates are included in the counts. This differs from
the behavior of the active add-ons measurement.

If no add-ons of a particular type are installed, the property for that type
will not be present (as opposed to an explicit property with value of 0).

Example
^^^^^^^

::

    "2013-03-14": {
      "org.mozilla.addons.counts": {
        "_v": 1,
        "extension": 21,
        "plugin": 4,
        "theme": 1
      }
    }



org.mozilla.appInfo.appinfo
---------------------------

This measurement contains basic XUL application and Gecko platform
information. It is reported in the *last* section.

Version 2
^^^^^^^^^

In addition to fields present in version 1, this version has the following
fields appearing in the *days* section:

isBlocklistEnabled
    Whether the blocklist ping is enabled. This is an integer, 0 or 1.
    This does not indicate whether the blocklist ping was sent but merely
    whether the application will try to send the blocklist ping.

isTelemetryEnabled
    Whether Telemetry is enabled. This is an integer, 0 or 1.

Version 1
^^^^^^^^^

The measurement object contains mostly string values describing the
current application and build. The properties are:

* vendor
* name
* id
* version
* appBuildID
* platformVersion
* platformBuildID
* os
* xpcomabi
* updateChannel
* distributionID
* distributionVersion
* hotfixVersion
* locale
* isDefaultBrowser

Notes
^^^^^

All of the properties appear in the *last* section except for
*isDefaultBrowser*, which appears under *days*.

Example
^^^^^^^

This example comes from an official OS X Nightly build::

    "org.mozilla.appInfo.appinfo": {
      "_v": 1,
      "appBuildID": "20130311030946",
      "distributionID": "",
      "distributionVersion": "",
      "hotfixVersion": "",
      "id": "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}",
      "locale": "en-US",
      "name": "Firefox",
      "os": "Darwin",
      "platformBuildID": "20130311030946",
      "platformVersion": "22.0a1",
      "updateChannel": "nightly",
      "vendor": "Mozilla",
      "version": "22.0a1",
      "xpcomabi": "x86_64-gcc3"
    },

org.mozilla.appInfo.update
--------------------------

This measurement contains information about the application update mechanism
in the application.

Version 1
^^^^^^^^^

The following daily values are reported:

enabled
    Whether automatic application update checking is enabled. 1 for yes,
    0 for no.
autoDownload
    Whether automatic download of available updates is enabled.

Notes
^^^^^

This measurement was merged to mozilla-central for JS FHR on 2013-07-15.

Example
^^^^^^^

::

    "2013-07-15": {
      "org.mozilla.appInfo.update": {
        "_v": 1,
        "enabled": 1,
        "autoDownload": 1,
      }
    }

org.mozilla.appInfo.versions
----------------------------

This measurement contains a history of application version numbers.

Version 2
^^^^^^^^^

Version 2 reports more fields than version 1 and is not backwards compatible.
The following fields are present in version 2:

appVersion
    An array of application version strings.
appBuildID
    An array of application build ID strings.
platformVersion
    An array of platform version strings.
platformBuildID
    An array of platform build ID strings.

When the application is upgraded, the new version and/or build IDs are
appended to their appropriate fields.

Version 1
^^^^^^^^^

When the application version (*version* from *org.mozilla.appinfo.appinfo*)
changes, we record the new version on the day the change was seen. The new
versions for a day are recorded in an array under the *version* property.

Notes
^^^^^

If the application isn't upgraded, this measurement will not be present.
This means this measurement will not be present for most days if a user is
on the release channel (since updates are typically released every 6 weeks).
However, users on the Nightly and Aurora channels will likely have a lot
of these entries since those builds are updated every day.

Values for this measurement are collected when performing the daily
collection (typically occurs at upload time). As a result, it's possible
the actual upgrade day may not be attributed to the proper day - the
reported day may lag behind.

The app and platform versions and build IDs should be identical for most
clients. If they are different, we are possibly looking at a *Frankenfox*.

Example
^^^^^^^

::

    "2013-03-27": {
      "org.mozilla.appInfo.versions": {
        "_v": 2,
        "appVersion": [
           "22.0.0"
        ],
        "appBuildID": [
          "20130325031100"
        ],
        "platformVersion": [
          "22.0.0"
        ],
        "platformBuildID": [
          "20130325031100"
        ]
      }
    }

org.mozilla.appSessions.current
-------------------------------

This measurement contains information about the currently running XUL
application's session.

Version 3
^^^^^^^^^

This measurement has the following properties:

startDay
    Integer days since UNIX epoch when this session began.
activeTicks
    Integer count of *ticks* the session was active for. Gecko periodically
    sends out a signal when the session is active. Session activity
    involves keyboard or mouse interaction with the application. Each tick
    represents a window of 5 seconds where there was interaction.
totalTime
    Integer seconds the session has been alive.
main
    Integer milliseconds it took for the Gecko process to start up.
firstPaint
    Integer milliseconds from process start to first paint.
sessionRestored
    Integer milliseconds from process start to session restore.

Example
^^^^^^^

::

    "org.mozilla.appSessions.current": {
      "_v": 3,
      "startDay": 15775,
      "activeTicks": 4282,
      "totalTime": 249422,
      "main": 851,
      "firstPaint": 3271,
      "sessionRestored": 5998
    }

org.mozilla.appSessions.previous
--------------------------------

This measurement contains information about previous XUL application sessions.

Version 3
^^^^^^^^^

This measurement contains per-day lists of all the sessions started on that
day. The following properties may be present:

cleanActiveTicks
    Active ticks of sessions that were properly shut down.
cleanTotalTime
    Total number of seconds for sessions that were properly shut down.
abortedActiveTicks
    Active ticks of sessions that were not properly shut down.
abortedTotalTime
    Total number of seconds for sessions that were not properly shut down.
main
    Time in milliseconds from process start to main process initialization.
firstPaint
    Time in milliseconds from process start to first paint.
sessionRestored
    Time in milliseconds from process start to session restore.

Notes
^^^^^

Sessions are recorded on the date on which they began.

If a session was aborted/crashed, the total time may be less than the actual
total time. This is because we don't always update total time during periods
of inactivity and the abort/crash could occur after a long period of idle,
before we've updated the total time.

The lengths of the arrays for {cleanActiveTicks, cleanTotalTime},
{abortedActiveTicks, abortedTotalTime}, and {main, firstPaint, sessionRestored}
should all be identical.

The length of the clean sessions plus the length of the aborted sessions should
be equal to the length of the {main, firstPaint, sessionRestored} properties.

It is not possible to distinguish the main, firstPaint, and sessionRestored
values from a clean vs aborted session: they are all lumped together.

For sessions spanning multiple UTC days, it's not possible to know which
days the session was active for. It's possible a week long session only
had activity for 2 days and there's no way for us to tell which days.

Example
^^^^^^^

::

    "org.mozilla.appSessions.previous": {
      "_v": 3,
      "cleanActiveTicks": [
        78,
        1785
      ],
      "cleanTotalTime": [
        4472,
        88908
      ],
      "main": [
        32,
        952
      ],
      "firstPaint": [
        2755,
        3497
      ],
      "sessionRestored": [
        5149,
        5520
      ]
    }

org.mozilla.crashes.crashes
---------------------------

This measurement contains a historical record of application crashes.

Version 2
^^^^^^^^^

The switch to version 2 coincides with the introduction of the
:ref:`crashes_crashmanager`, which provides a more robust source of
crash data.

This measurement will be reported on each day there was a crash. The
following fields may be present in each record:

mainCrash
   The number of main process crashes that occurred on the given day.

Yes, version 2 does not track submissions like version 1. It is very
likely submissions will be re-added later.

Also absent from version 2 are plugin crashes and hangs. These will be
re-added, likely in version 3.

Version 1
^^^^^^^^^

This measurement will be reported on each day there was a crash. The
following properties are reported:

pending
    The number of crash reports that haven't been submitted.
submitted
    The number of crash reports that were submitted.

Notes
^^^^^

Main process crashes are typically submitted immediately after they
occur (by checking a box in the crash reporter, which should appear
automatically after a crash). If the crash reporter submits the crash
successfully, we get a submitted crash. Else, we leave it as pending.

A pending crash does not mean it will eventually be submitted.

Pending crash reports can be submitted post-crash by going to
about:crashes.

If a pending crash is submitted via about:crashes, the submitted count
increments but the pending count does not decrement. This is because FHR
does not know which pending crash was just submitted and therefore it does
not know which day's pending crash to decrement.

Example
^^^^^^^

::

    "org.mozilla.crashes.crashes": {
      "_v": 1,
      "pending": 1,
      "submitted": 2
    },
    "org.mozilla.crashes.crashes": {
      "_v": 2,
      "mainCrash": 2
    }

org.mozilla.healthreport.submissions
------------------------------------

This measurement contains a history of FHR's own data submission activity.
It was added in Firefox 23 in early May 2013.

Version 2
^^^^^^^^^

This is the same as version 1 except an additional field has been added.

uploadAlreadyInProgress
   A request for upload was initiated while another upload was in progress.
   This should not occur in well-behaving clients. It (along with a lock
   preventing simultaneous upload) was added to ensure this never occurs.

Version 1
^^^^^^^^^

Daily counts of upload events are recorded.

firstDocumentUploadAttempt
    An attempt was made to upload the client's first document to the server.
    These are uploads where the client is not aware of a previous document ID
    on the server. Unless the client had disabled upload, there should be at
    most one of these in the history of the client.

continuationUploadAttempt
    An attempt was made to upload a document that replaces an existing document
    on the server. Most upload attempts should be attributed to this as opposed
    to *firstDocumentUploadAttempt*.

uploadSuccess
    The upload attempt recorded by *firstDocumentUploadAttempt* or
    *continuationUploadAttempt* was successful.

uploadTransportFailure
    An upload attempt failed due to transport failure (network unavailable,
    etc).

uploadServerFailure
    An upload attempt failed due to a server-reported failure. Ideally these
    are failures reported by the FHR server itself. However, intermediate
    proxies, firewalls, etc may trigger this depending on how things are
    configured.

uploadClientFailure
    An upload attempt failued due to an error/exception in the client.
    This almost certainly points to a bug in the client.

The result for an upload attempt is always attributed to the same day as
the attempt, even if the result occurred on a different day from the attempt.
Therefore, the sum of the result counts should equal the result of the attempt
counts.

org.mozilla.places.places
-------------------------

This measurement contains information about the Places database (where Firefox
stores its history and bookmarks).

Version 1
^^^^^^^^^

Daily counts of items in the database are reported in the following properties:

bookmarks
    Integer count of bookmarks present.
pages
    Integer count of pages in the history database.

Example
^^^^^^^

::

    "org.mozilla.places.places": {
      "_v": 1,
      "bookmarks": 388,
      "pages": 94870
    }

org.mozilla.profile.age
-----------------------

This measurement contains information about the current profile's age.

Version 1
^^^^^^^^^

A single *profileCreation* property is present. It defines the integer
days since UNIX epoch that the current profile was created.

Notes
^^^^^

It is somewhat difficult to obtain a reliable *profile born date* due to a
number of factors.

Example
^^^^^^^

::

    "org.mozilla.profile.age": {
      "_v": 1,
      "profileCreation": 15176
    }

org.mozilla.searches.counts
---------------------------

This measurement contains information about searches performed in the
application.

Version 2
^^^^^^^^^

This behaves like version 1 except we added all search engines that
Mozilla has a partner agreement with. Like version 1, we concatenate
a search engine ID with a search origin.

Another difference with version 2 is we should no longer misattribute
a search to the *other* bucket if the search engine name is localized.

The set of search engine providers is:

* amazon-co-uk
* amazon-de
* amazon-en-GB
* amazon-france
* amazon-it
* amazon-jp
* amazondotcn
* amazondotcom
* amazondotcom-de
* aol-en-GB
* aol-web-search
* bing
* eBay
* eBay-de
* eBay-en-GB
* eBay-es
* eBay-fi
* eBay-france
* eBay-hu
* eBay-in
* eBay-it
* google
* google-jp
* google-ku
* google-maps-zh-TW
* mailru
* mercadolibre-ar
* mercadolibre-cl
* mercadolibre-mx
* seznam-cz
* twitter
* twitter-de
* twitter-ja
* yahoo
* yahoo-NO
* yahoo-answer-zh-TW
* yahoo-ar
* yahoo-bid-zh-TW
* yahoo-br
* yahoo-ch
* yahoo-cl
* yahoo-de
* yahoo-en-GB
* yahoo-es
* yahoo-fi
* yahoo-france
* yahoo-fy-NL
* yahoo-id
* yahoo-in
* yahoo-it
* yahoo-jp
* yahoo-jp-auctions
* yahoo-mx
* yahoo-sv-SE
* yahoo-zh-TW
* yandex
* yandex-ru
* yandex-slovari
* yandex-tr
* yandex.by
* yandex.ru-be

And of course, *other*.

The sources for searches remain:

* abouthome
* contextmenu
* searchbar
* urlbar

The measurement will only be populated with providers and sources that
occurred that day.

If a user switches locales, searches from default providers on the older
locale will still be supported. However, if that same search engine is
added by the user to the new build and is *not* a default search engine
provider, its searches will be attributed to the *other* bucket.

Version 1
^^^^^^^^^

We record counts of performed searches grouped by search engine and search
origin. Only search engines with which Mozilla has a business relationship
are explicitly counted. All other search engines are grouped into an
*other* bucket.

The following search engines are explicitly counted:

* Amazon.com
* Bing
* Google
* Yahoo
* Other

The following search origins are distinguished:

about:home
    Searches initiated from the search text box on about:home.
context menu
    Searches initiated from the context menu (highlight text, right click,
    and select "search for...")
search bar
    Searches initiated from the search bar (the text field next to the
    Awesomebar)
url bar
    Searches initiated from the awesomebar/url bar.

Due to the localization of search engine names, non en-US locales may wrongly
attribute searches to the *other* bucket. This is fixed in version 2.

Example
^^^^^^^

::

    "org.mozilla.searches.counts": {
      "_v": 1,
      "google.searchbar": 3,
      "google.urlbar": 7
    },

org.mozilla.sync.sync
---------------------

This daily measurement contains information about the Sync service.

Values should be recorded for every day FHR measurements occurred.

Version 1
^^^^^^^^^

This version debuted with Firefox 30 on desktop. It contains the following
properties:

enabled
   Daily numeric indicating whether Sync is configured and enabled. 1 if so,
   0 otherwise.

preferredProtocol
   String version of the maximum Sync protocol version the client supports.
   This will be ``1.1`` for for legacy Sync and ``1.5`` for clients that
   speak the Firefox Accounts protocol.

actualProtocol
   The actual Sync protocol version the client is configured to use.

   This will be ``1.1`` if the client is configured with the legacy Sync
   service or if the client only supports ``1.1``.

   It will be ``1.5`` if the client supports ``1.5`` and either a) the
   client is not configured b) the client is using Firefox Accounts Sync.

syncStart
   Count of sync operations performed.

syncSuccess
   Count of sync operations that completed successfully.

syncError
   Count of sync operations that did not complete successfully.

   This is a measure of overall sync success. This does *not* reflect
   recoverable errors (such as record conflict) that can occur during
   sync. This is thus a rough proxy of whether the sync service is
   operating without error.

org.mozilla.sync.devices
------------------------

This daily measurement contains information about the device type composition
for the configured Sync account.

Version 1
^^^^^^^^^

Version 1 was introduced with Firefox 30.

Field names are dynamic according to the client-reported device types from
Sync records. All fields are daily last seen integer values corresponding to
the number of devices of that type.

Common values include:

desktop
   Corresponds to a Firefox desktop client.

mobile
   Corresponds to a Fennec client.

org.mozilla.sysinfo.sysinfo
---------------------------

This measurement contains basic information about the system the application
is running on.

Version 2
^^^^^^^^^

This version debuted with Firefox 29 on desktop.

A single property was introduced.

isWow64
   If present, this property indicates whether the machine supports WoW64.
   This property can be used to identify whether the host machine is 64-bit.

   This property is only present on Windows machines. It is the preferred way
   to identify 32- vs 64-bit support in that environment.

Version 1
^^^^^^^^^

The following properties may be available:

cpuCount
    Integer number of CPUs/cores in the machine.
memoryMB
    Integer megabytes of memory in the machine.
manufacturer
    The manufacturer of the device.
device
    The name of the device (like model number).
hardware
    Unknown.
name
    OS name.
version
    OS version.
architecture
    OS architecture that the application is built for. This is not the
    actual system architecture.

Example
^^^^^^^

::

    "org.mozilla.sysinfo.sysinfo": {
      "_v": 1,
      "cpuCount": 8,
      "memoryMB": 8192,
      "architecture": "x86-64",
      "name": "Darwin",
      "version": "12.2.0"
    }



org.mozilla.experiments.info
----------------------------------

Daily measurement reporting information about the Telemetry Experiments service.

Version 1
^^^^^^^^^

Property:

lastActive
    ID of the final Telemetry Experiment that is active on a given day, if any.


Example
^^^^^^^

::

    "org.mozilla.experiments.info": {
      "_v": 1,
      "lastActive": "some.experiment.id"
    }

