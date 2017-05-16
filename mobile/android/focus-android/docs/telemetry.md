# Firefox Focus for Android

## Telemetry

For clients that have "send anonymous usage data" enabled Focus sends a "core" ping and an "event" ping to Mozilla's telemetry service. Sending telemetry can be disabled in the app's settings. Builds of "Focus for Android" have telemetry enabled by default ("opt-out") while builds of "Klar for Android" have telemetry disabled by default.

### Core ping

Focus for Android creates and tries to send a "core" ping whenever the app goes to the background. This core ping uses the same format as Firefox for Android and is [documented on readthedocs.io](https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html).

### Event ping

In addition to the core ping an event ping for UI telemetry is generated and sent as soon as the app is sent to the background.

#### Settings

As part of the event ping the most recent state of the user's setting is sent:

* Search engine selection (`pref_search_engine`: search engine identifier)
* Block ad trackers (`pref_privacy_block_ads`: true/false)
* Block analytics trackers (`pref_privacy_block_other`: true/false)
* Block social trackers (`pref_privacy_block_social`: true/false)
* Block content trackers (`pref_privacy_block_analytics`: true/false)
* Block web fonts (`pref_performance_block_webfonts`: true/false)
* Block images (`pref_performance_block_images`: true/false)

#### Events

The event ping contains a list of events ([see event format on readthedocs.io](http://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/collection/events.html)) for the following actions:

* Start session - ("action", "foreground", "app")
* Stop session - ("action", "background", "app")
* (Browsing) URL entered: - ("action", "type_url", "search_bar")
* (Browsing) Incoming normal browsing intent from third-party app - ("action", "intent_url", "app")
* (Browsing) Incoming custom tab intent from third-party app - ("action", "intent_custom_tab", "app")
* (Browsing) Text selection action from third-party app ("action", "text_selection_intent", "app")
* (Browsing) Long press on image or link, or image in link ("action", "long_press", "browser")
* (BrowserContextMenu) Context menu dismissed without any selection ("action", "cancel", "browser_contextmenu")
* (BrowserContextMenu) Share Link menu item selected ("action", "share", "browser_contextmenu", "link")
* (BrowserContextMenu) Copy link menu item selected ("action", "copy", "browser_contextmenu", "link")
* (BrowserContextMenu) Share Image menu item selected ("action", "share", "browser_contextmenu", "image")
* (BrowserContextMenu) Copy Image menu item selected ("action", "copy", "browser_contextmenu", "image")
* (Search) Query entered: - ("action", "type_query", "search_bar")
* (Search) Query/Hint clicked: - ("action", "select_query", "search_bar")
* (Erase) UI button clicked - ("action", "click", "erase_button")
* (Erase) device’s back button pressed - ("action", "click", "back_button", "erase")
* (Erase) Notification clicked - ("action", "click", "notification", "erase")
* (Erase) Home screen shortcut clicked - ("action", "click", "shortcut", "erase")
* Enable/Disable content blocking for session - ("action", "click", "blocking_switch", "true/false")
* Setting changed: - ("action", "change", "settings", <key>, { "to": <value> })
* Share URL with third-party app - ("action", "share", "menu")
* Open default app for URL - ("action", "open", "menu", "default")
* Open Firefox directly - ("action", "open", "menu", "firefox")
* Open an app for this URL from a list of available apps - ("action", "open", "menu", "selection")

#### Limits

* An event ping will contain up to but no more than 500 events
* No more than 40 pings per type (core/event) are stored on disk for upload at a later time
* No more than 100 pings are sent per day

### Implementation notes

* Event pings are generated (and stored on disk) whenever the onStop() callback of the main activity is triggered. This happens whenever the main screen of the app is no longer visible (The app is in the background or another screen is displayed on top of the app).

* Whenever we are storing pings we are also scheduling an upload. We are using Android’s JobScheduler API for that. This allows the system to run the background task whenever it is convenient and certain criterias are met. The only criteria we are specifying is that we require an active network connection. In most cases this job is executed immediately after the app is in the background.

* Whenever an upload fails we are scheduling a retry. The first retry will happen after 30 seconds (or later if there’s no active network connection at this time). For further retries a exponential backoff policy is used: [30 seconds] * 2 ^ (num_failures - 1)

* An earlier retry of the upload can happen whenever the app is coming to the foreground and sent to the background again (the previous scheduled job is reset and we are starting all over again).
