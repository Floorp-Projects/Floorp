==============
Search Service
==============

This is documentation for the Search Service.

Definitions
===========

* Application-provided engine (aka app-provided): This is an engine that is
  provided by the application to the user as part of the configurations for the
  user's locale/region.
* Default engine: This is the engine that is the one used by default when
  doing searches from the address bar, search bar and other places. This may be
  a default application provided engine or a user selected engine.
* Default private engine: Same as for the default engine, but this is used by
  default when in private browsing mode.
* Application default engine: The engine selected by the application as default,
  in the absence of user settings. In the code, this is referred to as
  "Original".

Contents
========

.. toctree::
   :maxdepth: 2

   SearchEngineConfiguration
   SearchConfigurationSchema
   SearchEngines
   DefaultSearchEngines
   Preferences
   Telemetry
