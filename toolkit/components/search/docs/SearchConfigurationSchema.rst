===========================
Search Configuration Schema
===========================

.. note::
    This configuration is currently being implemented in `Bug 1542235`_.

This document outlines the details of the schema and how the various sub-parts
interact. For the full fields and descriptions, please see the `schema itself`_.

.. note::
    In the examples, only relevant properties are displayed.

Overview
========

The configuration is a JSON blob which is object with a `data` property which
is an array of engines:

.. code-block:: js

    {
      data: [
        {
          // engine 1 details
        },
        {
          // engine 2 details
        }
      ]
    }

Engine Objects
==============

An engine's details are located in the properties of the object associated with it.
An engine that is deployed globally could be listed simply as:

.. code-block:: js

    {
      "default": "no",
      "telemetryId": "engine1-telem",
      "webExtension": {
        "id": "web@ext",
        "version": "1.0"
      },
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

    {
      "webExtension": {
        "id": "web@ext",
        "version": "1.0"
      },
      "appliesTo": [{
        "included": {
          "region": "us"
        },
        "webExtension": {
          "id": "web-us@ext",
          "version": "1.1"
        }
      }, {
        "included": {
          "region": "gb"
        },
        "webExtension": {
          "id": "web-gb@ext",
          "version": "1.2"
        }
      }]
    }

In this case users identified as being in the US region would use the WebExtension
with identifier ``web-us@ext``, version 1.1. GB region users would get
``web-gb@ext`` version 1.2, and all other users would get ``web@ext`` version 1.0.

Special Attributes
==================

$USER_LOCALE
------------

If a ``webExtension.locales`` property contains an element with the value
``"$USER_LOCALE"`` then the special value will be replaced in the
configuration object with the users locale. For example:

.. code-block:: js

    {
      "webExtension": {
        "id": "web@ext",
        "version": "1.0"
      },
      "appliesTo": [{
        "included": {
          "locales": {
            "matches": ["us", "gb"]
          },
          "webExtension": {
            "locales": ["$USER_LOCALE"],
          }
        }
      }]
    }

Will report either ``[us]`` or ``[gb]`` as the ``webExtension.locales``
depending on the user's locale.

"default"
---------

You can specify ``"default"`` as a region in the configuration if
the engine is to be included when we do not know the user's region.

Application Scoping
===================

An engine configuration may be scoped to a particular application.

Channel
=======

One or more channels may be specified in an array to restrict a configuration
to just those channels. The current known channels are:

    - default: Self-builds of Firefox, or possibly some self-distributed versions.
    - nightly: Firefox Nightly builds.
    - aurora: Firefox Developer Edition
    - beta: Firefox Beta
    - release: The main Firefox release channel.
    - esr: The ESR Channel. This will also match versions of Firefox where the
      displayed version number includes ``esr``. We do this to include Linux
      distributions and other manual builds of ESR.

In the following example, ``web@ext`` would be set as default on the default
channel only, whereas ``web1@ext`` would be set as default on release and esr
channels.

.. code-block:: js

    {
      "webExtension": {
        "id": "web@ext"
      },
      "appliesTo": [{
        "included": {
          "everywhere": true
          "default": "yes",
          "application": {
            "channel": ["default"]
          }
        }
      ]}
    },
    {
      "webExtension": {
        "id": "web1@ext"
      },
      "appliesTo": [{
        "included": {
          "everywhere": true
          "default": "yes",
          "application": {
            "channel": ["release", "esr"]
          }
        }
      ]}
    }

Experiments
===========

We can run experiments by giving sections within ``appliesTo`` a
``cohort`` value, the Search Service can then optionally pass in a
matching ``cohort`` value to match those sections.

Sections which have a ``cohort`` will not be used unless a matching
``cohort`` has been passed in, for example:

.. code-block:: js

    {
      "webExtension": {
        "id": "web@ext",
        "version": "1.0"
      },
      "appliesTo": [{
        "included": {
          "everywhere": true
        },
        "cohort": "nov-16",
        "webExtension": {
          "id": "web-experimental@ext"
        }
      }, {
        "included": {
          "everywhere": true
        },
        "webExtension": {
          "id": "web-gb@ext",
          "version": "1.2"
        }
      }]
    }

Engine Defaults
===============

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

    {
      "webExtension": {
        "id": "engine1@ext",
        "version": "1.0"
      },
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
    {
      "webExtension": {
        "id": "engine2@ext",
        "version": "1.0"
      },
      "appliesTo": [{
        "included": {
          "region": "gb"
        },
        "default": "yes"
      }]
    },
      "webExtension": {
        "id": "engine3@ext",
        "version": "1.0"
      },
      "default": "no"
      "appliesTo": [{
        "included": {
          "everywhere": true
        },
      }]
    },
    {
      "webExtension": {
        "id": "engine4@ext",
        "version": "1.0"
      },
      "defaultPrivate": "yes",
      "appliesTo": [{
        "included": {
          "region": "fr"
        }
      }]
    }

In this example, for normal mode:

    - engine1@ext is default in the US region, and all other regions except for GB
    - engine2@ext is default in only the GB region
    - engine3@ext and engine4 are never default anywhere

In private browsing mode:

    - engine1@ext is default in the US region, and all other regions execpt for GB and FR
    - engine2@ext is default in only the GB region
    - engine3@ext is never default anywhere
    - engine4@ext is default in the FR region.

Engine Ordering
===============

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

    {
      "webExtension": {
        "id": "engine1@ext",
        "version": "1.0"
      },
      "orderHint": 2000,
      "default": "no",
    },
    {
      "webExtension": {
        "id": "engine2@ext",
        "version": "1.0"
      },
      "orderHint": 1000,
      "default": "yes"
    },
    {
      "webExtension": {
        "id": "engine3@ext",
        "version": "1.0"
      },
      "orderHint": 500,
      "default": "no"
    }

This would result in the order: ``engine2@ext, engine1@ext, engine3@ext``.

Engine Updates
==============

Within each engine definition is the extension id and version, for example:

.. code-block:: js

  {
      "webExtension": {
        "id": "web@ext",
        "version": "1.0"
      },
    }

To locate an engine to use, the Search Service will look in the following locations (in order):

#. within the user's install of the application.
#. in the configuration to see if there is an ``attachment`` field.

If the WebExtension is listed in the ``attachment``, then the app will download
to the user's profile, if it is not already there.

If an application is downloading the WebExtension, or it is not available, then
it may use an earlier version of the WebExtension until a new one becomes available.

.. _Bug 1542235: https://bugzilla.mozilla.org/show_bug.cgi?id=1542235
.. _schema itself: https://searchfox.org/mozilla-central/source/toolkit/components/search/schema/
