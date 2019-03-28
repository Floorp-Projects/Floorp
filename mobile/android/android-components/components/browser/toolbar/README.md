# [Android Components](../../../README.md) > Browser > Toolbar

A customizable toolbar for browsers.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-toolbar:{latest-version}"
```

### XML attributes

| Attribute                               |  Format   | Description                                             |
|-----------------------------------------|-----------|---------------------------------------------------------|
| browserToolbarHintColor                 | color     | Color of the text displayed when the URL is empty.      |
| browserToolbarTextColor                 | dimension | Color of the displayed URL.                             |
| browserToolbarTextSize                  | color     | Text size for the displayed URL and editable text.      |
| browserToolbarSecureColor               | color     | Color tint of the "secure" (lock) icon.                 |
| browserToolbarInsecureColor             | color     | Color tint of the "insecure" (globe) icon.              |
| browserToolbarMenuColor                 | color     | Color of the overflow menu button.                      |
| browserToolbarClearColor                | color     | Color of the editing clear text button.                 |
| browserToolbarSuggestionBackgroundColor | color     | Background color of the autocomplete suggestion.        |
| browserToolbarSuggestionForegroundColor | color     | Foreground (text) color of the autocomplete suggestion. |
| browserToolbarFadingEdgeSize            | dimension | Size of the fading edge shown when the URL is too long. |

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item    | Extras         | Description                        |
|--------|---------|----------------|------------------------------------|
| CLICK  | menu    |                | The user opened the overflow menu. |
| COMMIT | toolbar | `commitExtras` | The user has edited the URL.       |

#### `commitExtras`

| Key          | Type    | Value                             |
|--------------|---------|-----------------------------------|
| autocomplete | Boolean | Whether the URL was autocompleted |
| source       | String? | Which autocomplete list was used  |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
