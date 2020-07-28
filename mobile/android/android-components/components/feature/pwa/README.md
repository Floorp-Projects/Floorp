# [Android Components](../../../README.md) > Feature > Progressive Web Apps (PWA)

Feature implementation for Progressive Web Apps (PWA).

- https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps
- https://developer.mozilla.org/en-US/docs/Web/Manifest

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-pwa:{latest-version}"
```

### Adding feature to application

#### Creating basic homescreen shortcuts

`WebAppUseCases` includes a use case to pin websites to the homescreen. When called, the method will show a prompt to the user to let them drag a shortcut with the currently selected session onto their homescreen.

If you don't want to support full web apps and only want the shortcut functionality, set the `supportWebApps` parameter to `false` when creating `WebAppUseCases`. This causes all shortcuts to open a new tab in the browser.

#### Opening web apps

To open the pinned shortcut as a progressive web app, add `mozilla.components.feature.pwa.PWA_LAUNCHER` to your intent filter.

```xml
<intent-filter>
    <action android:name="mozilla.components.feature.pwa.VIEW_PWA" />
    <category android:name="android.intent.category.DEFAULT" />
    <data android:scheme="https" />
</intent-filter>
```

You must also process the intent with the `WebAppIntentProcessor`. This processor will create a new session with the `webAppManifest` and `customTabConfig` fields set.

The web app manifest will also be serialized onto the intent as a JSON string extra. It can be retrieved using the `Intent.getWebAppManifest` extension function.

#### Hiding the toolbar

`WebAppHideToolbarFeature` is used to hide the toolbar view when the user is visiting the website tied to the shortcut. Once they navigate away, the feature will show the toolbar again.

This functionality pairs well with the `CustomTabsToolbarFeature`, which can be used to show a custom tab toolbar instead of a regular toolbar.

#### Immersive mode, orientation settings, and recents entries.

`WebAppActivityFeature` will set activity-level settings corresponding to the web app.

The recents screen will show the icon and title of the web app.

The activity orientation will be restricted to match `"orientation"` in the web app manifest.

When `"display": "fullscreen"` is set in the web app manifest, the web app will be displayed in immersive mode.

#### Displaying controls without a toolbar

`WebAppSiteControlsFeature` will display a silent notification whenever a web app is open. This notification contains controls to interact with the web app, such as a refresh button and a shortcut to copy the URL.

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item    | Extras         | Description                        |
|--------|---------|----------------|------------------------------------|
| CLICK  | install_shortcut  |   | The user installs a PWA shortcut. |
| CLICK  | homescreen_icon_tap  |   | The user tapped the PWA icon on the homescreen. |

#### `itemExtras`

| Key          | Type    | Value                             |
|--------------|---------|-----------------------------------|
| timingNs     | Long | The current system time when a foreground or background action is taken. |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
