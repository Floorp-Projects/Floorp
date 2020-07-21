# Trigger Listeners

A set of action listeners that can be used to trigger CFR messages.

## How to update

Make a pull request against [mozilla/nimbus-shared](https://github.com/mozilla/nimbus-shared/) repo with your changes.
Build and copy over resulting schema from `nimbus-shared/schemas/messaging/` to `toolkit/components/messaging-system/schemas/TriggerActionSchemas.json`.

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

### `newSavedLogin`

Happens every time the user saves or updates a login via the login capture doorhanger.
Provides a `type` to diferentiate between the two events that can be used in targeting.

Does not filter by host or patterns.

```typescript
let type = "update" | "save";
```

### `contentBlocking`

Happens every time there is a content blocked event. Provides a context of the
number of pages loaded in the current browsing session that can be used in targeting.

Does not filter by host or patterns.

```typescript
let pageLoad = number;
```
