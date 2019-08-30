===========================
Search Engine Configuration
===========================

.. note::
    This configuration is currently being implemented in `Bug 1542235`_.

Configuration Management
========================

The application stores a dump of the configuration that is used for first
initialisation. Subsequent updates to the configuration are either updates to the
static dump, or they may be served via remote servers.

The mechanism of delivering the dumps and settings to the Search Service is
`Remote Settings`_

Remote settings
---------------

The remote settings bucket for the search engine configuration list is
``search-config``. The version that is currently being delivered
to clients can be `viewed live`_.

Configuration Schema
====================

The configuration format is defined via a `JSON schema`_. The search engine
configuration schema is `stored in mozilla-central`_ and is uploaded to the
Remote Settings server at convenient times after it changes.

This document outlines the details of the schema and how the various sub-parts
interact. For the current fields and descriptions, please see the `schema itself`_.

Configuration Structure
=======================

.. note::
    In the examples, only relevant properties are displayed.

Overview
--------

The configuration is a JSON blob which has a list of keys which are the engine name:

.. code-block:: js

    {
      "engine1": {
        ...
      },
      "engine2": {
        ...
      }
    }

.. note::
    The engine name is intended only as a key/identifier.

Engine Objects
--------------

An engine's details are located in the properties of the object associated with it.
An engine that is deployed globally could be listed simply as:

.. code-block:: js

    "engine1": {
      "default": "no",
      "telemetryId": "engine1",
      "webExtensionId": "webext",
      "webExtensionVersion": "1.0",
      "appliesTo": [{
        "included": {
          "everywhere": true
        }
      }]
    }

The ``appliesTo`` section is an array of objects. At least one object is required
to specify which regions/locales the engine is included within. If an
``appliesTo`` object lists additional attributes then these will override any
attributes at the top-level.

For example, a more complex engine definition may be available only to users
located specific regions or with certain locales. For example:

.. code-block:: js

    "engine1": {
      "webExtensionId": "webext",
      "webExtensionVersion": "1.0",
      "appliesTo": [{
        "included": {
          "region": "us"
        },
        "webExtensionId": "webext-us",
        "webExtensionVersion": "1.1"
      }, {
        "included": {
          "region": "gb"
        },
        "webExtensionId": "webext-gb",
        "webExtensionVersion": "1.2"
      }]
    }

In this case users identified as being in the US region would use the WebExtension
with identifier ``webext-engine1``, version 1.1. GB region users would get
``webext-gb`` version 1.2, and all other users would get ``webext`` version 1.0.

Special Attributes
------------------

If a ``webExtensionLocale`` attribute is specified with the value
``"$USER_LOCALE"`` then the special value will be replaced in the
configuration object with the users locale. For example:

.. code-block:: js

    "engine1": {
      "webExtensionId": "webext",
      "webExtensionVersion": "1.0",
      "appliesTo": [{
        "included": {
          "locales": {
            "matches": ["us", "gb"]
          },
          "webExtensionLocale": "$USER_LOCALE",
        },

Will report either ``us`` or ``gb`` as the ``webExtensionLocale``
depending on the user.

Engine Defaults
---------------

An engine may be specified as the default for one of two purposes:

#. normal browsing mode,
#. private browsing mode.

If there is no engine specified for private browsing mode for a particular region/locale
pair, then the normal mode engine is used.

If the instance of the application does not support a separate private browsing mode engine,
then it will only use the normal mode engine.

An engine may or may not be default for particular regions/locales. The ``default``
property is a tri-state value with states of ``yes``, ``yes-if-no-other`` and
``no``. Here's an example of how they apply:

.. code-block:: js

    "engine1": {
      "appliesTo": [{
        "included": {
          "region": "us"
        },
        "default": "yes"
      }, {
        "excluded": {
          "region": "us"
        },
        "default": "yes-if-no-other"
      }]
    },
    "engine2": {
      "appliesTo": [{
        "included": {
          "region": "gb"
        },
        "default": "yes"
      }]
    },
    "engine3": {
      "default": "no"
      "appliesTo": [{
        "included": {
          "everywhere": true
        },
      }]
    },
    "engine4": {
      "defaultPrivate": "yes",
      "appliesTo": [{
        "included": {
          "region": "fr"
        }
      }]

In this example, for normal mode:

    - engine1 is default in the US region, and all other regions except for GB
    - engine2 is default in only the GB region
    - engine3 and engine4 are never default anywhere

In private browsing mode:

    - engine1 is default in the US region, and all other regions execpt for GB and FR
    - engine2 is default in only the GB region
    - engine3 is never default anywhere
    - engine4 is default in the FR region.

Engine Ordering
---------------

The ``orderHint`` field indicates the suggested ordering of an engine relative to
other engines when displayed to the user, unless the user has customized their
ordering.

The default ordering of engines is based on a combination of if the engine is
default, and the ``orderHint`` fields. The ordering is structured as follows:

#. Default engine in normal mode
#. Default engine in private browsing mode (if different from the normal mode engine)
#. Other engines in order from the highest ``orderHint`` to the lowest.

Example:

.. code-block:: js

    "engine1": {
      "orderHint": 2000,
      "default": "no",
    },
    "engine2": {
      "orderHint": 1000,
      "default": "yes"
    },
    "engine3": {
      "orderHint": 500,
      "default": "no"
    }

This would result in the order: ``engine2, engine1, engine3``.

Engine Updates
--------------

Within each engine definition is the extension id and version, for example:

.. code-block:: js

    "engine": {
      "webExtensionId": "webext",
      "webExtensionVersion": "1.0",
    }

To locate an engine to use, the Search Service will look in the following locations (in order):

#. within the user's install of the application.
#. in the configuration to see if there is an ``attachment`` field.

If the WebExtension is listed in the ``attachment``, then the app will download
to the user's profile, if it is not already there.

If an application is downloading the WebExtension, or it is not available, then
it may use an earlier version of the WebExtension until a new one becomes available.

.. _Bug 1542235: https://bugzilla.mozilla.org/show_bug.cgi?id=1542235
.. _Remote Settings: /services/common/services/RemoteSettings
.. _JSON schema: https://json-schema.org/
.. _stored in mozilla-central:
.. _schema itself: https://searchfox.org/mozilla-central/source/toolkit/components/search/schema/
.. _viewed live: https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-engine-configuration/records
