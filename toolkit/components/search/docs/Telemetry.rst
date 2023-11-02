Telemetry
=========

This document describes search telemetry recorded by Toolkit such as search
service telemetry and telemetry related to fetching search suggestions.

Other important search-related telemetry is recorded by Firefox and is
documented in :doc:`/browser/search/telemetry` in the Firefox documentation.

Legacy Telemetry
----------------

Scalars
-------

browser.searchinit.secure_opensearch_engine_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of secure (i.e., using https) OpenSearch search
  engines a given user has installed.

browser.searchinit.insecure_opensearch_engine_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of insecure (i.e., using http) OpenSearch search
  engines a given user has installed.

browser.searchinit.secure_opensearch_update_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of OpenSearch search engines with secure updates
  enabled (i.e., using https) a given user has installed.

browser.searchinit.insecure_opensearch_update_count
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the number of OpenSearch search engines with insecure updates
  enabled (i.e., using http) a given user has installed.

Keyed Scalars
-------------

browser.searchinit.engine_invalid_webextension
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Records the WebExtension ID of a search engine where the saved search engine
  settings do not match the WebExtension.

  The keys are the WebExtension IDs. The values are integers:

  1. Associated WebExtension is not installed.
  2. Associated WebExtension is disabled.
  3. The submission URL of the associated WebExtension is different to that of the saved settings.

Histograms
----------

SEARCH_SUGGESTIONS_LATENCY_MS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  This histogram records the latency in milliseconds of fetches to the
  suggestions endpoints of search engines, or in other words, the time from
  Firefox's request to a suggestions endpoint to the time Firefox receives a
  response. It is a keyed exponential histogram with 50 buckets and values
  between 0 and 30000 (0s and 30s). Keys in this histogram are search engine IDs
  for built-in search engines and 'other' for non-built-in search engines.

Default Search Engine
~~~~~~~~~~~~~~~~~~~~~
Telemetry for the user's default search engine is currently reported via two
systems:

  1. Legacy telemetry:
     `Fields are reported within the legacy telemetry environment <https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/data/environment.html#defaultsearchengine>`__
  2. Glean:
     `Fields are documented in the Glean dictionary <https://dictionary.telemetry.mozilla.org/apps/firefox_desktop?search=search.engine>`__.

Glean Telemetry
---------------
`These search service fields are documented via Glean dictionary <https://dictionary.telemetry.mozilla.org/apps/firefox_desktop?page=1&search=search.service>`__.

search.service.startup_time
~~~~~~~~~~~~~~~~~~~~~~~~~~~

  The time duration it takes for the search service to start up.

search.service.initializaitonStatus
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  A labeled counter for the type of initialization statuses that can occur on
  start up. Labels include: ``failedSettings``, ``failedFetchEngines``,
  ``failedLoadEngines``, ``success``.

  A counter for initialization successes on start up.
