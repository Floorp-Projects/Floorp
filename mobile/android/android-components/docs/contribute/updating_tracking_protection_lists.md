---
layout: page
title: Updating the tracking protecting lists process
permalink: /contributing/update-tracking-protection-list
---

## What is tracking protection?

It is a feature that prevents trackers from collecting your personal information like your browsing habits and interests. In `GeckoEngine` it comes [built-in in Gecko(View)](https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/ContentBlockingController.ContentBlockingException.html). For the `SystemEngine` we have to [created our own implementation](https://github.com/vladikoff/android-components/blob/main/components/browser/engine-system/src/main/java/mozilla/components/browser/engine/system/matcher/UrlMatcher.kt),
which relays on different lists that indicates what should be considered as a tracker or not.

## The lists

The following lists are kept outside of the Android Components repository in the [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists) repository.


| AC                    | Shavar                     | Purpose                                                                                                                                                                                                |
|-----------------------|----------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [domain_blocklist.json][1] | [disconnect-blocklist.json][2]  | This blocklist is the core of tracking protection in Firefox. [For more information][3].                                                                                                     |
| [domain_safelist.json][4]  | [disconnect-entitylist.json][5] | It is used to allow third-party subresources that are wholly owned by the same company that owns the top-level website that the user is visiting for more details. [For more information][6].|



## Updating process

For every FireFox release, a new branch gets created following the same pattern as [merge-day](https://mozac.org/contributing/merge-day),
where `main` contains `nightly`'s lists, the higher branch number contains `Beta` and the higher branch number before contains `Stable`.
That means that every [merge-day](https://mozac.org/contributing/merge-day), we need to update the [domain_blocklist.json][1] and [domain_safelist.json][4] files with their counterpart on [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists).

### Preparation
1. Go to the [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists) repository
2. Switch to the higher branch number (Beta).
3. Copy the content of [disconnect-blocklist.json][2] -> [domain_blocklist.json][1]
4. Run all the tests on `browser-engine-system` 🤞
5. 🔴 `If` something fails proceed to verify what's causing the issue and address it.
6. ✅ `Else` proceed to copy the content of [disconnect-entitylist.json][5] -> [domain_safelist.json][4]
7. Run all the tests on `browser-engine-system` 🤞
8. 🔴 `If` something fails proceed to verify what's causing the issue and address it.
9. ✅ `Else` proceed to commit your changes using the guideline below.

To keep track of every update, we include the [shavar-prod-lists](https://github.com/mozilla-services/shavar-prod-lists) commit hash as part of our commit message.
This way we can know which version of the lists we have on AC.

```
Closes #6163: Update tracking protection lists

domain_blocklist.json maps to disconnect-blocklist.json
commit(shavar-prod-lists) d5755856f4eeab4ce5e8fb7896600ed7768368e5

domain_safelist.json maps to disconnect-entitylist.json
commit(shavar-prod-lists) 01dcca911aa7787fd835a1a19cef1012296f4eb7
```

THE END!

[1]: https://github.com/mozilla-mobile/android-components/blob/main/components/browser/engine-system/src/main/res/raw/domain_blocklist.json
[2]: https://github.com/mozilla-services/shavar-prod-lists/blob/master/disconnect-blocklist.json
[3]: https://github.com/mozilla-services/shavar-prod-lists/blob/master/README.md#disconnect-blocklist.json
[4]: https://github.com/mozilla-mobile/android-components/blob/main/components/browser/engine-system/src/main/res/raw/domain_safelist.json
[5]: https://github.com/mozilla-services/shavar-prod-lists/blob/master/disconnect-entitylist.json
[6]: https://github.com/mozilla-services/shavar-prod-lists/blob/master/README.md#disconnect-entitylistjson
