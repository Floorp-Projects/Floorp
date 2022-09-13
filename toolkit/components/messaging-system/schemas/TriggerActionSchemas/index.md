# Trigger Listeners

A set of action listeners that can be used to trigger CFR messages.

## Usage

[As part of the CFR definition](https://searchfox.org/mozilla-central/rev/2bfe3415fb3a2fba9b1c694bc0b376365e086927/browser/components/newtab/lib/CFRMessageProvider.jsm#194) the message can register at most one trigger used to decide when the message is shown.

Most triggers (unless otherwise specified) take the same arguments of `hosts` and/or `patterns`
used to target the message to specific websites.

```javascript
// Optional set of hosts to filter out triggers only to certain websites
let params: string[];
// Optional set of [match patterns](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns) to filter out triggers only to certain websites
let patterns: string[];
```

```javascript
{
  ...
  // Show the message when opening mozilla.org
  "trigger": { "id": "openURL", "params": ["mozilla.org", "www.mozilla.org"] }
  ...
}
```

```javascript
{
  ...
  // Show the message when opening any HTTP, HTTPS URL.
  trigger: { id: "openURL", patterns: ["*://*/*"] }
  ...
}
```

## Available trigger actions

### `openArticleURL`

Happens when the user loads a Reader Mode compatible webpage.

### `openBookmarkedURL`

Happens when the user bookmarks or navigates to a bookmarked URL.

Does not filter by host or patterns.

### `frequentVisits`

Happens every time a user navigates (or switches tab to) to any of the `hosts` or `patterns` arguments
provided. Additionally it stores timestamps of these visits that are provided back to the targeting context.
They can be used inside of the targeting expression:

```javascript
// Has at least 3 visits in the past hour
recentVisits[.timestamp > (currentDate|date - 3600 * 1000 * 1)]|length >= 3

```

```typescript
interface visit {
  host: string,
  timestamp: UnixTimestamp
};
// Host and timestamp for every visit to "Host"
let recentVisits: visit[];
```

### `openURL`

Happens every time the user loads a new URL that matches the provided `hosts` or `patterns`.
During a browsing session it keeps track of visits to unique urls that can be used inside targeting expression.

```javascript
// True on the third visit for the URL which the trigger matched on
visitsCount >= 3
```

### `newSavedLogin`

Happens every time the user saves or updates a login via the login capture doorhanger.
Provides a `type` to diferentiate between the two events that can be used in targeting.

Does not filter by host or patterns.

```typescript
let type = "update" | "save";
```

### `contentBlocking`

Happens at the and of a document load and for every subsequent content blocked event.
Provides a context of the number of pages loaded in the current browsing session that can be used in targeting.

Does not filter by host or patterns.

The event it reports back is a flag or a combination of flags merged together by
ANDing the various STATE_BLOCKED_* flags.

```typescript
// https://searchfox.org/mozilla-central/rev/2fcab997046ba9e068c5391dc7d8848e121d84f8/uriloader/base/nsIWebProgressListener.idl#260
let event: ContentBlockingEventFlag;
let pageLoad = number;
```

### `defaultBrowserCheck`

Happens at startup, when opening a newtab and when navigating to about:home.
At startup it provides the result of running `DefaultBrowserCheck.willCheckDefaultBrowser` to follow existing behaviour if needed.
On the newtab/homepage it reports the `source` as `newtab`.

```typescript
let source = "newtab" | undefined;
let willShowDefaultPrompt = boolean;
```

### `captivePortalLogin`

Happens when the user successfully goes through a captive portal authentication flow.

### `preferenceObserver`

Watch for changes on any number of preferences. Runs when a pref is added, removed or modified.

```js
// Register a message with the following trigger
{
  id: "preferenceObserver",
  params: ["pref name"]
}
```

### `featureCalloutCheck`

Happens when navigating to about:firefoxview or other about pages with Feature Callout tours enabled

### `nthTabClosed`

Happens when the user closes n or more tabs in a session

```js
// Register a message with the following trigger id and
// include the tabsClosedCount context variable in the targeting.
// Here, the message triggers after two or more tabs are closed.
{
  id: "nthTabClosed",
  targeting: "tabsClosedCount >= 2"
}
```
