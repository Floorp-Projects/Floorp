# User Actions

A subset of actions are available to messages via fields like `action` on buttons for CFRs.

## Usage

For CFRs, you should add the action `type` in `action` and any additional parameters in `data`. For example:

```json
"action": {
  "type": "OPEN_PREFERENCES_PAGE",
  "data": { "category": "sync" },
}
```

## Available Actions

### `OPEN_APPLICATIONS_MENU`

* args: (none)

Opens the applications menu.

### `OPEN_FIREFOX_VIEW`

* args: (none)

Opens the Firefox View pseudo-tab.

### `OPEN_PRIVATE_BROWSER_WINDOW`

* args: (none)

Opens a new private browsing window.


### `OPEN_URL`

* args: `string` (a url)

Opens a given url.

Example:

```json
"action": {
  "type": "OPEN_URL",
  "data": { "args": "https://foo.com" },
}
```

### `OPEN_ABOUT_PAGE`

* args:
```ts
{
  args: string, // (a valid about page without the `about:` prefix)
  entrypoint?: string, // URL search param used for referrals
}
```

Opens a given about page

Example:

```json
"action": {
  "type": "OPEN_ABOUT_PAGE",
  "data": { "args": "privatebrowsing" },
}
```

### `OPEN_PREFERENCES_PAGE`

* args:
```
{
  args?: string, // (a category accessible via a `#`)
  entrypoint?: string // URL search param used to referrals

Opens `about:preferences` with an optional category accessible via a `#` in the URL (e.g. `about:preferences#home`).

Example:

```json
"action": {
  "type": "OPEN_PREFERENCES_PAGE",
  "data": { "category": "general-cfrfeatures" },
}
```

### `SHOW_FIREFOX_ACCOUNTS`

* args: (none)

Opens Firefox accounts sign-up page. Encodes some information that the origin was from snippets by default.

### `FXA_SIGNIN_FLOW`

* args:

```ts
{
  // a valid `where` value for `openUILinkIn`. Only `tab` and `window` have been tested, and `tabshifted`
  // is unlikely to do anything different from `tab`.
  where?: "tab" | "window" = "tab",

  entrypoint?: string // URL search params string to pass along to FxA. Defaults to "activity-stream-firstrun".
  extraParams?: object // Extra parameters to pass along to FxA. See FxAccountsConfig.promiseConnectAccountURI.
}
```

Opens a Firefox accounts sign-up or sign-in page, and does the work of closing the resulting tab or window once
sign-in completes. Returns a Promise that resolves to `true` if sign-in succeeded, or to `false` if the sign-in
window or tab closed before sign-in could be completed.

Encodes some information that the origin was from about:welcome by default.


### `SHOW_MIGRATION_WIZARD`

* args: (none)

Opens import wizard to bring in settings and data from another browser.

### `PIN_CURRENT_TAB`

* args: (none)

Pins the currently focused tab.

### `HIGHLIGHT_FEATURE`

Can be used to highlight (show a light blue overlay) a certain button or part of the browser UI.

* args: `string` a [valid targeting defined in the UITour](https://searchfox.org/mozilla-central/rev/7fd1c1c34923ece7ad8c822bee062dd0491d64dc/browser/components/uitour/UITour.jsm#108)

### `INSTALL_ADDON_FROM_URL`

Can be used to install an addon from addons.mozilla.org.

* args:
```ts
{
  url: string,
  telemetrySource?: string
};
```

### `OPEN_PROTECTION_REPORT`

Opens `about:protections`

### `OPEN_PROTECTION_PANEL`

Opens the protection panel behind on the lock icon of the awesomebar

### `DISABLE_STP_DOORHANGERS`

Disables all Social Tracking Protection messages

* args: (none)

### `OPEN_AWESOME_BAR`

Focuses and expands the awesome bar.

* args: (none)

### `CANCEL`

No-op action used to dismiss CFR notifications (but not remove or block them)

* args: (none)

### `DISABLE_DOH`

User action for turning off the DoH feature

* args: (none)

### `ACCEPT_DOH`

User action for continuing to use the DoH feature

* args: (none)

### `CONFIGURE_HOMEPAGE`

Action for configuring the user homepage and restoring defaults.

* args:
```ts
{
  homePage: "default" | null;
  newtab: "default" | null;
  layout: {
    search: boolean;
    topsites: boolean;
    highlights: boolean;
    topstories: boolean;
  }
}
```

### `PIN_FIREFOX_TO_TASKBAR`

Action for pinning Firefox to the user's taskbar.

* args: (none)

### `SET_DEFAULT_BROWSER`

Action for setting the default browser to Firefox on the user's system.

- args: (none)

### `SET_DEFAULT_PDF_HANDLER`

Action for setting the default PDF handler to Firefox on the user's system.

Windows only.

- args:
```ts
{
  // Only set Firefox as the default PDF handler if the current PDF handler is a
  // known browser.
  onlyIfKnownBrowser?: boolean;
}
```

### `DECLINE_DEFAULT_PDF_HANDLER`

Action for declining to set the default PDF handler to Firefox on the user's
system. Prevents the user from being asked again about this.

Windows only.

- args: (none)

### `SHOW_SPOTLIGHT`

Action for opening a spotlight tab or window modal using the content passed to the dialog.

### `BLOCK_MESSAGE`

Disable a message by adding to an indexedDb list of blocked messages

* args: `string` id of the message

### `SET_PREF`

Action for setting various browser prefs

Prefs that can be changed with this action are:

- `browser.dataFeatureRecommendations.enabled`
- `browser.migrate.content-modal.about-welcome-behavior`
- `browser.migrate.content-modal.import-all.enabled`
- `browser.migrate.preferences-entrypoint.enabled`
- `browser.shopping.experience2023.active`
- `browser.shopping.experience2023.optedIn`
- `browser.shopping.experience2023.survey.optedInTime`
- `browser.shopping.experience2023.survey.hasSeen`
- `browser.shopping.experience2023.survey.pdpVisits`
- `browser.startup.homepage`
- `browser.startup.windowsLaunchOnLogin.disableLaunchOnLoginPrompt`
- `browser.privateWindowSeparation.enabled`
- `browser.firefox-view.feature-tour`
- `browser.pdfjs.feature-tour`
- `browser.newtab.feature-tour`
- `cookiebanners.service.mode`
- `cookiebanners.service.mode.privateBrowsing`
- `cookiebanners.service.detectOnly`
- `messaging-system.askForFeedback`

Any pref that begins with `messaging-system-action.` is also allowed.
Alternatively, if the pref is not present in the list above and does not begin
with `messaging-system-action.`, it will be created and prepended with
`messaging-system-action.`. For example, `example.pref` will be created as
`messaging-system-action.example.pref`.

* args:
```ts
{
  pref: {
    name: string;
    value: string | boolean | number;
  }
}
```

### `MULTI_ACTION`

Action for running multiple actions. Actions should be included in an array of actions.

* args:
```ts
{
  actions: Array<UserAction>
}
```

* example:
```json
"action": {
  "type": "MULTI_ACTION",
  "data": {
    "actions": [
      {
        "type": "OPEN_URL",
        "args": "https://www.example.com"
      },
      {
        "type": "OPEN_AWESOME_BAR"
      }
    ]
  }
}
```

### `CLICK_ELEMENT`

* args: `string` A CSS selector for the HTML element to be clicked

Selects an element in the current Window's document and triggers a click action


### `RELOAD_BROWSER`

* args: (none)

Action for reloading the current browser.


### `FOCUS_URLBAR`

Focuses the urlbar in the window the message was displayed in

* args: (none)
