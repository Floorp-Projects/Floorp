===========================
Search Engine Configuration
===========================

The search engine configuration is a mapping that is used to determine the
list of search engines for each user. The mapping is primarily based on the
user's region and locale.

Configuration Management
========================

The application stores a dump of the configuration that is used for first
initialisation. Subsequent updates to the configuration are either updates to the
static dump, or they may be served via remote servers.

The mechanism of delivering the settings dumps to the Search Service is
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

An outline of the schema may be found on the `Search Configuration Schema`_ page.

Updating Search Engine WebExtensions
====================================

Updates for application provided search engine WebExtensions are provided via
`Normandy`_.

It is likely that updates for search engine WebExtensions will be
received separately to configuration updates which may or may not be directly
related. As a result several situations may occur:

    - The updated WebExtension is for an app-provided engine already in-use by
      the user.

      - In this case, the search service will apply the changes to the
        app-provided engine's data.

    - A WebExtension addition/update is for an app-provided engine that is not
      in-use by the user, or not in the configuration.

      - In this case, the search service will ignore the WebExtension.
      - If the configuration (search or user) is updated later and the
        new engine is added, then the Search Service will start to use the
        new engine.

    - A configuration update is received that needs a WebExtension that is
      not found locally.

      - In this case, the search service will ignore the missing engine and
        continue without it.
      - When the WebExtension is delivered, the search engine will then be
        installed and added.

.. _Remote Settings: /services/common/services/RemoteSettings.html
.. _JSON schema: https://json-schema.org/
.. _stored in mozilla-central: https://searchfox.org/mozilla-central/source/toolkit/components/search/schema/
.. _Search Configuration Schema: SearchConfigurationSchema.html
.. _viewed live: https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-config/records
.. _Normandy: /toolkit/components/normandy/normandy/services.html
