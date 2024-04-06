===========================
Search Engine Configuration
===========================

The search engine configuration is a mapping that is used to determine the
list of search engines for each user. The mapping is primarily based on the
user's region and locale.

Configuration Management
========================

The configuration is delivered and managed via `remote settings`_. There are
:searchfox:`dumps <services/settings/dumps/main/>` of the configuration
that are shipped with the application, for use on first startup of a fresh profile,
or when a client has not been able to receive remote settings updates for
whatever reason.

Remote Settings Bucket
----------------------

The remote settings bucket for the search engine configuration list is
``search-config-v2``. The version that is currently being delivered
to clients can be `viewed live`_. There are additional remote settings buckets
with information for each search engine. These buckets are listed below.

- `search-config-icons`_ is a mapping of icons to a search engine.
- `search-config-overrides-v2`_ contains information that may override engines
  properties in search-config-v2.

Configuration Schema
====================

The configuration format is defined via a `JSON schema`_. The search engine
configuration schema is `stored in mozilla-central`_ and is uploaded to the
Remote Settings server at convenient times after it changes.

An outline of the schemas may be found on the `Search Configuration Schema`_ page.

.. _remote settings: /services/settings/index.html
.. _JSON schema: https://json-schema.org/
.. _stored in mozilla-central: https://searchfox.org/mozilla-central/source/toolkit/components/search/schema/
.. _Search Configuration Schema: SearchConfigurationSchema.html
.. _viewed live: https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-config-v2/records
.. _search-config-icons: https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-config-icons/records
.. _search-config-overrides-v2: https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-config-overrides-v2/records
.. _search-default-override-allowlist: https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/search-default-override-allowlist/records
