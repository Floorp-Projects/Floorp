Preferences
===========

This document describes Preferences affecting the toolkit Search Service.
Preferences that are generated and updated by code won't be described here.

User Exposed
------------
These preferences are exposed through the Firefox UI.

browser.search.suggest.enabled (default: true)
  Whether search suggestions are enabled in the search bar. Turning this off
  also currently prevents search suggestions in the address bar.

Hidden
------
These preferences are normally hidden, and should not be used unless you really
know what you are doing.

browser.search.log (boolean, default: false)
  Whether search service console logging is enabled or not.

browser.search.suggest.timeout (number, default: 500)
  When requesting suggestions from a search server, it has this long to respond.

browser.search.update (boolean, default: true)
  Whether updates are enabled for OpenSearch based engines. This only applies
  to OpenSearch engines where their definition supplies an update url.
  Note that this does not affect any of: application updates, add-on updates,
  remote settings updates.

Experimental
------------
These preferences are experimental and not officially supported. They could be
removed at any time.

browser.search.separatePrivateDefault.ui.enabled (boolean, default: false)
  Whether the UI is enabled for having a separate default search engine in
  private browsing mode.

browser.search.separatePrivateDefault (boolean, default: false)
  Whether the user has selected to have a separate default search engine in
  private browsing mode.

browser.search.suggest.enabled.private (boolean, default: false)
  Whether search suggestions are enabled in a private browsing window.
