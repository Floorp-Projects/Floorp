# Suggest

The **Suggest Rust component** powers the [Firefox Suggest](https://support.mozilla.org/en-US/kb/firefox-suggest-faq) feature.

This component currently supports the basic Suggest experience only. The basic experience shows suggestions for sponsored and web content from a canned dataset. The component downloads the dataset from [Remote Settings](https://remote-settings.readthedocs.io/en/latest/), stores the suggestions in a local database, and makes them available to the Firefox address bar. Because matching is done locally, Mozilla never sees the user's query.

The opt-in "Improved Firefox Suggest Experience", which sends user queries to a [Mozilla-owned proxy server](https://mozilla-services.github.io/merino/intro.html) for server-side matching, is not currently supported.
