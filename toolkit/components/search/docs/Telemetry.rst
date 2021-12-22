=========
Telemetry
=========

This document describes search telemetry recorded by Toolkit such as search
service telemetry and telemetry related to fetching search suggestions.

Other important search-related telemetry is recorded by Firefox and is
documented in :doc:`/browser/search/telemetry` in the Firefox documentation.

Scalars
-------

browser.searchinit.init_result_status_code
  Records the search service initialization code on startup. This is typically
  one of the error values in https://searchfox.org/mozilla-central/source/xpcom/base/ErrorList.py

Keyed Scalars
-------------

browser.searchinit.engine_invalid_webextension
  Records the WebExtension ID of a search engine where the saved search engine
  settings do not match the WebExtension.

  The keys are the WebExtension IDs. The values are integers:

  1. Associated WebExtension is not installed.
  2. Associated WebExtension is disabled.
  3. The submission URL of the associated WebExtension is different to that of the saved settings.

Histograms
----------

SEARCH_SUGGESTIONS_LATENCY_MS
  This histogram records the latency in milliseconds of fetches to the
  suggestions endpoints of search engines, or in other words, the time from
  Firefox's request to a suggestions endpoint to the time Firefox receives a
  response. It is a keyed exponential histogram with 50 buckets and values
  between 0 and 30000 (0s and 30s). Keys in this histogram are search engine IDs
  for built-in search engines and 'other' for non-built-in search engines.
