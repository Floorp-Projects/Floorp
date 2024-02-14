# Content blocking

## Outdated Content.
Focus is using the same technology as Firefox for desktop, all the content blocking is happening inside of the Gecko engine web engine. content blocking list can be seen here https://github.com/mozilla-services/shavar-prod-lists.

## Tracking protection: lists and general overview

We use the disconnect tracking protection lists, these consist of:

- The deny-list: a list of domains to block
- The entitylist: an override list to unblock certain domains for certain other domains.
- The google_mapping: a list of modifications for the deny-list.

### The deny-list

The deny-list contains list of domains that should be blocked. Any resources that are hosted
on these domains should be blocked.

The source lists contain multiple categories, we use **Advertising**, **Social**, **Analytics**, **Content** (aka. "other content trackers").
We use some items from the **Disconnect** list, those items are moved into the **Social** list when parsing. We ignore **Legacy Disconnect** and
**Legacy Social**. You can see the code parsing these lists at
[BlockListProcessor.java](../app/src/webkit/java/org/mozilla/focus/webkit/matcher/BlocklistProcessor.java).

The google_mapping is similar to the main list - these items are simply addded to the existing categories mentioned above. (The google entries
in the main **Disconnect** list are discarded since we use those from google_mapping.json.)

Note: each category contains a list of site owners: those names are discarded, we insert every single domain for every owner into the same list.

### Entitylist: the override list

Some domains need to be unblocked when visiting another domain belonging to the same owner. E.g. we usually block "googleapis.com", but when visiting e.g. google.de,
we unblock "googleapis.com". This is done in the entitylist: for every domain that belongs to a "property" in the entitylist, we unblock all domains listed in
"resources".

## Implementation

WebView calls the WebViewClient.shouldInterceptRequest() callback every time a resource is to be loaded - this permits us to intercept resource loading, and is how we
can perform content blocking on Android. (See [BlockListProcessor.java](../app/src/webkit/java/org/mozilla/focus/webkit/TrackingProtectionWebViewClient.java) ).

We then just need to verify every resource URL to determine whether it can be loaded in that callback: we use a custom trie-based domain matching implementation for
Focus for Android. This is different from Focus iOS: Focus for iOS was originally a content blocking safari plugin, and used the iOS content blocking API
( https://developer.apple.com/library/content/documentation/Extensions/Conceptual/ContentBlockingRules/Introduction/Introduction.html ).
That API uses a specific blocklist format, therefore firefox-ios converts the disconnect lists into the iOS format at build-time.

Focus-ios browser was implemented later, and reuses the iOS content blocking list format (iOS webview was unable to make use of the content blocking API
at that time, and therefore implemented its own URL matching). This is a regex based format: Focus-ios (browser) therefore stores a list of regexes, and iterates
over that list to check whether a given resource URL matches. That approach means that focus-ios only needs one copy of the blocklists, however this isn't ideal for performance.

As mentioned, Focus for Android uses a custom Trie implementation instead of iterating over regexes. This does mean that we aren't reusing iOS's blocklist
format, but it also permits for ~140x faster domain lookup when compared to a port of the iOS domain lookup implementation. The entitylist is similar,
and we use extended versions of the same Trie for the domain overrides. See [UrlMatcher](../app/src/webkit/java/org/mozilla/focus/webkit/matcher/UrlMatcher.java) for
the actual matcher implementation.


## Miscellaneous notes
- We do not make any internet connection because every blocklist is built into Firefox Focus. When you enable a blocklsit in settings, our app will load the selected blocklist from disk. We will provide updated lists as an "App update".
- We discard the site owners and names when parsing the blocklists, that makes it harder to keep track of which trackers have been blocked for a given site. That data would probably have to be added to the blocklist trie if we want a better breakdown of trackers.
- Our Trie search is recursive, and uses a slightly silly "TrieString" to take care of string reversal and substrings. An iterative implementation would probably be better, and would avoid substrings. Such an implementation would be slightly more complex since we would have to keep track of string indexes, but reduces overhead. Given that current performance is acceptable, there isn't huge value in actually working on this.
