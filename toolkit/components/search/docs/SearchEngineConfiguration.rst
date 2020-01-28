===========================
Search Engine Configuration
===========================

.. note::
    This configuration is currently being implemented in `Bug 1542235`_.

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

Search Engine WebExtension updates will be delivered by Normandy.

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

.. _Bug 1542235: https://bugzilla.mozilla.org/show_bug.cgi?id=1542235
.. _Remote Settings: /services/common/services/RemoteSettings
.. _JSON schema: https://json-schema.org/
.. _stored in mozilla-central: https://searchfox.org/mozilla-central/source/toolkit/components/search/schema/
.. _Search Configuration Schema: SearchConfigurationSchema.html
.. _viewed live: https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-config/records
