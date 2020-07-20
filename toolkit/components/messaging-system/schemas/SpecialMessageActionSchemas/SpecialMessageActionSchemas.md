# User Actions

A subset of actions are available to messages via fields like `button_action` for snippets, or `primary_action` for CFRs.

## Usage

For snippets, you should add the action type in `button_action` and any additional parameters in `button_action_args. For example:

```json
{
  "button_action": "OPEN_ABOUT_PAGE",
  "button_action_args": "config"
}
```

## How to update

Make a pull request against [mozilla/nimbus-shared](https://github.com/mozilla/nimbus-shared/) repo with your changes.
Build and copy over resulting schema from `nimbus-shared/schemas/messaging/` to `toolkit/components/messaging-system/schemas/SpecialMessageActionSchemas.json`.

## Available Actions

### `OPEN_APPLICATIONS_MENU`

* args: (none)

Opens the applications menu.

### `OPEN_PRIVATE_BROWSER_WINDOW`

* args: (none)

Opens a new private browsing window.


### `OPEN_URL`

* args: `string` (a url)

Opens a given url.

Example:

```json
{
  "button_action": "OPEN_URL",
  "button_action_args": "https://foo.com"
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
{
  "button_action": "OPEN_ABOUT_PAGE",
  "button_action_args": "config"
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
{
  "button_action": "OPEN_PREFERENCES_PAGE",
  "button_action_args": "home"
}
```

### `SHOW_FIREFOX_ACCOUNTS`

* args: (none)

Opens Firefox accounts sign-up page. Encodes some information that the origin was from snippets by default.

### `SHOW_MIGRATION_WIZARD`

* args: (none)

Opens import wizard to bring in settings and data from another browser.

### `PIN_CURRENT_TAB`

* args: (none)

Pins the currently focused tab.

### `ENABLE_FIREFOX_MONITOR`

* args:
```ts
{
  url: string;
  flowRequestParams: {
    entrypoint: string;
    utm_term: string;
    form_type: string;
  }
}
```

Opens an oauth flow to enable Firefox Monitor at a given `url` and adds Firefox metrics that user given a set of `flowRequestParams`.

#### `url`

The URL should start with `https://monitor.firefox.com/oauth/init` and add various metrics tags as search params, including:

* `utm_source`
* `utm_campaign`
* `form_type`
* `entrypoint`

You should verify the values of these search params with whoever is doing the data analysis (e.g. Leif Oines).

#### `flowRequestParams`

These params are used by Firefox to add information specific to that individual user to the final oauth URL. You should include:

* `entrypoint`
* `utm_term`
* `form_type`

The `entrypoint` and `form_type` values should match the encoded values in your `url`.

You should verify the values with whoever is doing the data analysis (e.g. Leif Oines).

#### Example

```json
{
  "button_action": "ENABLE_FIREFOX_MONITOR",
  "button_action_args": {
     "url": "https://monitor.firefox.com/oauth/init?utm_source=snippets&utm_campaign=monitor-snippet-test&form_type=email&entrypoint=newtab",
      "flowRequestParams": {
        "entrypoint": "snippets",
        "utm_term": "monitor",
        "form_type": "email"
      }
  }
}
```

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
