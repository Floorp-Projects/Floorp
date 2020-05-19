# Telemetry
For clients that have "send anonymous usage data" enabled Focus sends a "core" ping and an "event" ping to Mozilla's telemetry service. Sending telemetry can be disabled in the app's settings. Builds of "Focus for Android" have telemetry enabled by default ("opt-out") while builds of "Klar for Android" have telemetry disabled by default.

# Core ping

Focus for Android creates and tries to send a "core" ping whenever the app goes to the background. This core ping uses the same format as Firefox for Android and is [documented on firefox-source-docs.mozilla.org](https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/core-ping.html).

# Event ping

In addition to the core ping an event ping for UI telemetry is generated and sent as soon as the app is sent to the background.

## Settings

As part of the event ping the most recent state of the user's setting is sent (default values in **bold**):

| Setting                  | Key                             | Value(*)
|--------------------------|---------------------------------|----------------------
| Default search engine    | pref_search_engine              | **bundled engine name**/`custom`(**)
| Block ad trackers        | pref_privacy_block_ads          | **true**/false
| Block analytics trackers | pref_privacy_block_analytics    | **true**/false
| Block social trackers    | pref_privacy_block_social       | **true**/false
| Block content trackers   | pref_privacy_block_other        | true/**false**
| Block web fonts          | pref_performance_block_webfonts | true/**false**
| Block images             | pref_performance_block_images   | true/**false**
| Locale override          | pref_locale                     | **empty string**/`locale-code`(***)
| Default browser          | pref_default_browser            | true/**false**
| Stealth mode             | pref_secure                     | **true**/false
| Autocomplete (Default)   | pref_autocomplete_preinstalled  | true/false
| Autocomplete (Custom)    | pref_autocomplete_custom        | true/false
|  Show tips on Firefox Focus home screen        | pref_key_tips          | **true**/false

(*) All values are sent as a `String`.

(**) If the default is one of the bundled engines, the name of the engine. Otherwise, just `custom`.

(***) An **empty string** value indicates "System Default" locale is selected.

## Events

The event ping contains a list of events ([see event format on readthedocs.io](https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/collection/events.html)) for the following actions:

### Sessions

| Event                                    | category | method     | object | value  |
|------------------------------------------|----------|------------|--------|--------|
| Start session (App is in the foreground) | action   | foreground | app    |        |
| Stop session (App is in the background)  | action   | background | app    |        |

### Browsing

| Event                                  | category | method                | object     | value  | extras.    |
|----------------------------------------|----------|-----------------------|------------|--------|------------|
| URL entered                            | action   | type_url              | search_bar |        | `search_suggestion: true/false` |
| Search entered                         | action   | type_query            | search_bar |        |            |
| Search hint clicked ("Search for ..")  | action   | select_query          | search_bar |        |            |
| Link from third-party app              | action   | intent_url            | app        |        |            |
| Text selection action from app         | action   | text_selection_intent | app        |        |            |
| Long press on image / link             | action   | long_press            | browser    |        |            |
| "Pull to refresh"                      | action   | swipe                 | browser    | reload |            |
| Autofill popup is shown                | action   | show                  | autofill   |        |            |
| Autofill performed                     | action   | click                 | autofill   |        |            |
| Enable Search Suggestion from prompt                            | action   | click              | search_suggestion_prompt |    true/false    |  |


(*) `options` is a JSON map containing:

```JavaScript
{
  "autocomplete": true,  // Was the URL autocompleted?
  "source":  "default",  // Which autocomplete list was used ("default" or "custom")?  (Only present if autocomplete is true)
  "total": 25            // Total number of items in the used autocomplete list (Only present if autocomplete is true)
}
```

### Custom Tabs

| Event                     | category | method            | object               | value      | extra      |
|---------------------------|----------|-------------------|----------------------|------------|------------|
| Custom tab opened         | action   | intent_custom_tab | app                  |            | `options`* |
| "Close" button clicked    | action   | click             | custom_tab_close_bu  |            | `tabs`*    |
| "Action" button clicked   | action   | click             | custom_tab_action_bu |            |            |
| Custom menu item selected | action   | open              | menu                 | custom_tab |            |

(*) `options` is a JSON map of enabled custom tab options as requested by the third-party app, e.g. [hasToolbarColor, hasCloseButton]

(*) `tabs` is a JSON map containing the position of the currently selected tab and the total number of open tabs:

```JavaScript
{
  "selected": "2"  // Currently selected tab (Zero-based numbering; -1 if no tab is selected)
  "total": "5"     // Total number of open tabs
}
```

### Context Menu

| Event                                  | category | method | object              | value      |
|----------------------------------------|----------|--------|---------------------|------------|
| Link context menu dismissed            | action   | cancel | browser_contextmenu | link       |
| Share Link menu item selected          | action   | share  | browser_contextmenu | link       |
| Copy link menu item selected           | action   | copy   | browser_contextmenu | link       |
| Image context menu dismissed           | action   | cancel | browser_contextmenu | image      |
| Share Image menu item selected         | action   | share  | browser_contextmenu | image      |
| Copy Image menu item selected          | action   | copy   | browser_contextmenu | image      |
| Save Image menu item selected          | action   | save   | browser_contextmenu | image      |
| Image with Link context menu dismissed | action   | cancel | browser_contextmenu | image+link |
| Open link in new tab                   | action   | open   | browser_contextmenu | tab        |
| Open new tab in Focus from customtab context menu | action   | open | browser_contextmenu |  full_browser   |



### Erasing session

| Event                                  | category | method      | object              | value      | extras  |
|----------------------------------------|----------|-------------|---------------------|------------|---------|
| Floating action button clicked         | action   | click       | erase_button        |            | `tabs`* |
| Back button clicked: Home screen       | action   | click       | back_button         | erase_home | `tabs`* |
| Back button clicked: Previous app      | action   | click       | back_button         | erase_app  | `tabs`* |
| Notification clicked                   | action   | click       | notification        | erase      | `tabs`* |
| Notification "Erase and Open" click    | action   | click       | notification_action | erase_open | `tabs`* |
| Home screen shortcut clicked           | action   | click       | shortcut            | erase      | `tabs`* |
| Erase history in tabs tray             | action   | click       | tabs_tray           | erase      | `tabs`* |
| App removed by user from "recent apps" | action   | click       | recent_apps         | erase      | `tabs`* |

(*) `tabs` is a JSON map containing the position of the currently selected tab and the total number of open tabs:

```JavaScript
{
  "selected": "2"  // Currently selected tab (Zero-based numbering; -1 if no tab is selected)
  "total": "5"     // Total number of open tabs
}
```

### Menu

| Event                                       | category | method   | object          | value        | extras  |
|---------------------------------------------|----------|----------|-----------------|--------------|----------|
| Enable/Disable content blocking for session | action   | click    | blocking_switch | true/false   |          |
| Share URL with third-party app              | action   | share    | menu            |              |          |
| Open default app for URL                    | action   | open     | menu            | default      |          |
| Open Firefox                                | action   | open     | menu            | firefox      |          |
| Open with ... (Show app chooser)            | action   | open     | menu            | selection    |          |
| Open full browser from custom tab           | action   | click    | menu            | full_browser |          |
| "Download Firefox" clicked (in app chooser) | action   | install  | app             | firefox      |          |
| "What's new"                                | action   | click    | menu            | whats_new    | `state`* |
| Reload current page                         | action   | click    | menu            | reload       |          |
| Report site issue                           | action   | click    | menu            | report_issue |          |
| Find in Page                                | action   | click    | menu            | find_in_page |          |
| Request desktop site                        | action   | click    | desktop_request_check| true/false |          |

(*) `state` is a JSON map containing information about whether the menu item was highlighted:

```JavaScript
{
  "highlighted":  "true" // Whether the menu item was highlighted or not
}
```

### Notification

| Event                                 | category | method   | object              | value      | extras  |
|---------------------------------------|----------|----------|---------------------|------------|---------|
| Notification clicked (Erase)          | action   | click    | notification        | erase      |         |
| Action "Open" clicked                 | action   | click    | notification_action | open       |         |
| Action "Erase and Open" clicked       | action   | click    | notification_action | erase_open | `tabs`* |

(*) `tabs` is a JSON map containing the position of the currently selected tab and the total number of open tabs:

```JavaScript
{
  "selected": "2"  // Currently selected tab (Zero-based numbering; -1 if no tab is selected)
  "total": "5"     // Total number of open tabs
}
```
### Page Load Time Histogram


| Event                    | category | method     | object | extras|
|-----------------|----------|--------------|---------| ----- |
| Histogram for Page Load Times for Foreground Session | histogram   | foreground  | browser   | histogram*|

(*) There are 200 extras attached to this event, each a bucket of 100 ms each, with the key as the minimum value in the bucket and the value as the corresponding number of events in the bucket. Anything over 20,000 is put in the last bucket. For example:
```
{”0":"2"}
{“100”:"3"}
...
{"19900", "4"}
```

### URI Count

| Event                                       | category | method   | object              | extra           |
|---------------------------------------------|----------|----------|---------------------|-----------------|
| The count of the total non-unique http(s) URIs visited in a subsession, including page reloads, after the session has been restored. This does not include background page requests and URIs from embedded pages or private browsing                    | action   | open    | browser            | `{"total_uri_count": num }`           |
| The count of the unique domains visited in a subsession, after the session has been restored. Subdomains under eTLD are aggregated after the first level (i.e. test.example.com and other.example.com are only counted once). This does not include background page requests and domains from embedded pages or private browsing.         | action   | open    | browser            | `{"unique_domains_count": num }`         |


### Downloads

| Event                                       | category | method   | object              | value           |
|---------------------------------------------|----------|----------|---------------------|-----------------|
| Clicked "Download"                          | action   | click    | download_dialog     | download        |
| Clicked "Cancel"                            | action   | click    | download_dialog     | cancel          |

### Open Focus From Icon

| Event                                       | category | method   | object              | value           |
|---------------------------------------------|----------|----------|---------------------|-----------------|
| Launched Focus From Icon                    | action   | click    | app_icon            | open            |
| Resume Focus From Icon                      | action   | click    | app_icon            | resume          |

### Add to Home screen

| Event                                   | category | method   | object                   | value             |
|-----------------------------------------|----------|----------|--------------------------|-------------------|
| Clicked "Add to Home screen" in dialog  | action   | click    | add_to_homescreen_dialog | add_to_homescreen |
| Clicked "Cancel" in dialog              | action   | click    | add_to_homescreen_dialog | cancel            |
| Open Focus from Home screen shortcut    | action   | click    | homescreen_shortcut      | open              |

### Share to Focus Event

| Event                                       | category | method   | object              | value           |
|---------------------------------------------|----------|----------|---------------------|-----------------|
| Shared URL to Focus                         | action   | share_intent  | app            | url             |
| Shared Search Terms to Focus                | action   | share_intent  | app            | search          |

### Settings

| Event                          | category | method   | object                  | value | extras               |
|--------------------------------|----------|----------|-------------------------|-------|--------------------------
| Setting changed                | action   | change   | setting                 | <key> | `{ "to": <value> }`
| Autocomplete domain added      | action   | save     | autocomplete_domain     |       | `{ "source": <value> }`
| Autocomplete domain removed    | action   | remove   | autocomplete_domain     |       | `{ "total": 5 }`
| Autocomplete domain reordered  | action   | reorder  | autocomplete_domain     |       | `options*`
| Open Exceptions Setting        | action   | open     | allowlist               |       |
| Remove Exceptions Domains      | action   | remove   | allowlist               |       |`{ "total": 5 }`
| Remove All Exceptions Domains  | action   | remove_all |allowlist              |       |
| Default search engine clicked  | action   | open     | search_engine_setting   |       |
| Change default search engine   | action   | save     | search_engine_setting   |       | `{"source": src* }`
| Select "Remove" engines screen | action   | remove   | search_engine_setting   |       |
| Delete search engines          | remove   | remove   | remove_search_engines   |       |`{"selected": num* }`
| Restore bundled engines        | action   | restore  | search_engine_setting   |       |
| Select "Add another engine"    | action   | show     | custom_search_engine    |       |
| Save custom search engine      | action   | save     | custom_search_engine    |       | `{"success": bool* }`
| Click "Add search engine" ℹ️   | action   | click    | search_engine_learn_more|       |
| Open with default browser prompt| action  | show     | make_default_browser_open_with|       |
| Settings default browser prompt| action   | show    | make_default_browser_settings|       |


(*) `options` is a JSON map containing:

```JavaScript
{
  "from": 5
  "to":   2
}
```

(*) `src` can be either `bundled` or `custom`

(*) `num` number of engines selected for deletion

(*) `bool` true if successfully saved, false if validation error

### Firstrun

| Event                                       | category | method   | object              | value        |
|---------------------------------------------|----------|----------|---------------------|--------------|
| Showing a first run page                    | action   | show     | firstrun            | `page`*      |
| Skip button pressed                         | action   | click    | firstrun            | skip         |
| Finish button pressed                       | action   | click    | firstrun            | finish       |

(*) Page numbers start at 0. Initially when the firstrun tour is shown an event for the first page (0) is fired.

### Multitasking / Tabs

| Event                                      | category | method   | object              | value | extras  |
|--------------------------------------------|----------|----------|---------------------|-------|---------|
| Context menu: Open link in new tab         | action   | open     | browser_contextmenu | tab   | `tabs`* |
| Open the tabs tray                         | action   | show     | tabs_tray           |       |         |
| Close the tabs tray (back / click outside) | action   | hide     | tabs_tray           |       |         |
| Switch to tab in tabs tray                 | action   | click    | tabs_tray           | tab   | `tabs`* |
| Erase history in tabs tray                 | action   | click    | tabs_tray           | erase | `tabs`* |

(*) `tabs` is a JSON map containing the position of the currently selected tab and the total number of open tabs:

```JavaScript
{
  "selected": "2"  // Currently selected tab (Zero-based numbering; -1 if no tab is selected)
  "total": "5"     // Total number of open tabs
}
```

### Homescreen Tips

| Event                                    | category | method     | object | value  |
|------------------------------------------|----------|------------|--------|--------|
| Open in new tab tip displayed | action   | show | tip | open_in_new_tab_tip    |        |
| Add to homescreen tip displayed  | action   | show | tip | add_to_homescreen_tip    |        |
| Disable tracking protection tip displayed  | action   | show | tip | disable_tracking_protection_tip    |     |
| Disable tips on home screen tip displayed  | action   | show | tip | disable_tips_tip    |    |
| Set default browser tip displayed  | action   | show | tip | default_browser_tip    |        |
| Autocomplete URL tip displayed | action   | show | tip | add_autocomplete_url_tip    |        |
| Open in new tab tip tapped | action   | click | tip | open_in_new_tab_tip    |        |
| Add to homescreen tip tapped  | action   | click | tip | add_to_homescreen_tip    |        |
| Disable tips tip tapped  | action   | click | tip | disable_tips_tip    |        | |
| Set default browser tip tapped  | action   | click | tip | default_browser_tip    |        |
| Autocomplete URL tip tapped | action   | click | tip | add_autocomplete_url_tip    |        |
| Homescreen tips enabled/disabled | action   | click | tip | add_to_homescreen_tip    |        |
| Survey tip displayed | action   | show | tip | survey_tip    |        |
| Survey tip tapped | action   | click | tip | survey_tip    |        |
| Survey (es) tip displayed | action   | show | tip | survey_tip_es    |        |
| Survey (es) tip tapped | action   | click | tip | survey_tip_es    |        |
| Survey (fr) tip displayed | action   | show | tip | survey_tip_fr    |        |
| Survey (fr) tip tapped | action   | click | tip | survey_tip_fr    |        |

### SSL Errors

| Event                                      | category | method   | object  | extras  |
|--------------------------------------------|----------|----------|---------|---------|
| SSL Error From Page                        | error    | page     | browser |`error`*|
| SSL Error From Resource                    | error    | resource | browser |`error`* |

(*)`error` is a JSON map containing the primary SSL Error

```JavaScript
{
  "error_code": "SSL_DATE_INVALID"  // Primary SSL Error
}
```

| Possible Error Codes |
|----------------------|
| SSL_DATE_INVALID     |
| SSL_EXPIRED          |
|SSL_IDMISMATCH        |
|SSL_NOTYETVALID       |
|SSL_UNTRUSTED         |
|SSL_INVALID           |
|Undefined SSL Error   |

## Limits

* An event ping will contain up to but no more than 500 events
* No more than 40 pings per type (core/event) are stored on disk for upload at a later time
* No more than 100 pings are sent per day

# Implementation notes

* Event pings are generated (and stored on disk) whenever the onStop() callback of the main activity is triggered. This happens whenever the main screen of the app is no longer visible (The app is in the background or another screen is displayed on top of the app).

* Whenever we are storing pings we are also scheduling an upload. We are using Android’s JobScheduler API for that. This allows the system to run the background task whenever it is convenient and certain criterias are met. The only criteria we are specifying is that we require an active network connection. In most cases this job is executed immediately after the app is in the background.

* Whenever an upload fails we are scheduling a retry. The first retry will happen after 30 seconds (or later if there’s no active network connection at this time). For further retries a exponential backoff policy is used: [30 seconds] * 2 ^ (num_failures - 1)

* An earlier retry of the upload can happen whenever the app is coming to the foreground and sent to the background again (the previous scheduled job is reset and we are starting all over again).
