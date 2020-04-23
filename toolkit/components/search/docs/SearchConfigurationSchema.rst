===========================
Search Configuration Schema
===========================

.. note::
    This configuration is currently under testing for nightly builds only, see
    `Bug 1542235`_ for more status information.

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
        "id": "web@ext"
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
        "id": "web@ext"
      },
      "appliesTo": [{
        "included": {
          "region": "us"
        },
        "webExtension": {
          "id": "web-us@ext"
        }
      }, {
        "included": {
          "region": "gb"
        },
        "webExtension": {
          "id": "web-gb@ext"
        }
      }]
    }

In this case users identified as being in the US region would use the WebExtension
with identifier ``web-us@ext``. GB region users would get
``web-gb@ext``, and all other users would get ``web@ext``.

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
        "id": "web@ext"
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

"override"
----------

The `"override"` field can be set to true if you want a section to
only override otherwise included engines. For example:

.. code-block:: js

    {
      "webExtension": {
        "id": "web@ext"
      },
      "appliesTo": [{
        // Complicated and lengthy inclusion rules
      }, {
        "override": true,
        "application": { "distributions": ["mydistrocode"]},
        "params": {
          "searchUrlGetParams": [
            { "name": "custom", "value": "foobar" }
          ]
        }
      }]
    }

Application Scoping
===================

An engine configuration may be scoped to a particular application.

Name
----

One or more application names may be specified. Currently the only application
type supported is ``firefox``. If an application name is specified, then it
must be matched for the section to apply. If there are no application names
specified, then the section will match any consumer of the configuration.

In the following example, ``web@ext`` would be included on any consumer
of the configuration, but ``web1@ext`` would only be included on Firefox desktop.

.. code-block:: js

    {
      "webExtension": {
        "id": "web@ext"
      },
      "appliesTo": [{
        "included": {
          "everywhere": true
          "application": {
            "name": []
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
          "application": {
            "name": ["firefox"]
          }
        }
      ]}
    }

Channel
-------

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

Distributions
-------------

Distributions may be specified to be included or excluded in an ``appliesTo``
section. The ``distributions`` field in the ``application`` section is an array
of distribution identifiers. The identifiers match those supplied by the
``distribution.id`` preference.

In the following, ``web@ext`` would be included in only the ``cake``
distribution. ``web1@ext`` would be excluded from the ``apples`` distribution
but included in the main desktop application, and all other distributions.

.. code-block:: js

    {
      "webExtension": {
        "id": "web@ext"
      },
      "appliesTo": [{
        "included": {
          "everywhere": true
          "application": {
            "distributions": ["cake"]
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
          "application": {
            "excludedDistributions": ["apples"]
          }
        }
      ]}
    }

Version
-------

Minimum and Maximum versions may be specified to restrict a configuration to
specific ranges. These may be open-ended. Version comparison is performed
using `the version comparator`_.

Note: comparison against ``maxVersion`` is a less-than comparison. The
``maxVersion`` won't be matched directly.

In the following example, ``web@ext`` would be included for any version after
72.0a1, whereas ``web1@ext`` would be included only between 68.0a1 and 71.x
version.

.. code-block:: js

    {
      "webExtension": {
        "id": "web@ext"
      },
      "appliesTo": [{
        "included": {
          "everywhere": true
          "application": {
            "minVersion": "72.0a1"
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
            "minVersion": "68.0a1"
            "maxVersion": "72.0a1"
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
        "id": "web@ext"
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
          "id": "web-gb@ext"
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
        "id": "engine1@ext"
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
        "id": "engine2@ext"
      },
      "appliesTo": [{
        "included": {
          "region": "gb"
        },
        "default": "yes"
      }]
    },
      "webExtension": {
        "id": "engine3@ext"
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
        "id": "engine4@ext"
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
        "id": "engine1@ext"
      },
      "orderHint": 2000,
      "default": "no",
    },
    {
      "webExtension": {
        "id": "engine2@ext"
      },
      "orderHint": 1000,
      "default": "yes"
    },
    {
      "webExtension": {
        "id": "engine3@ext"
      },
      "orderHint": 500,
      "default": "no"
    }

This would result in the order: ``engine2@ext, engine1@ext, engine3@ext``.

.. _Bug 1542235: https://bugzilla.mozilla.org/show_bug.cgi?id=1542235
.. _schema itself: https://searchfox.org/mozilla-central/source/toolkit/components/search/schema/
.. _the version comparator: https://developer.mozilla.org/en-US/docs/Mozilla/Toolkit_version_format
