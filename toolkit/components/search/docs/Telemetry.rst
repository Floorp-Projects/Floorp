=========
Telemetry
=========

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
