# [Android Components](../../../README.md) > Feature > Firefox Suggest

A component for accessing Firefox Suggest search suggestions.

[Firefox Suggest](https://support.mozilla.org/en-US/kb/firefox-suggest-faq) provides suggestions for sponsored and web content in the address bar. Suggestions are downloaded, stored, and matched on-device.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-fxsuggest:{latest-version}"
```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action      | Item                       | Extras            | Description                     |
|-------------|----------------------------|-------------------|---------------------------------|
| Click | amp_suggestion_clicked |  | a Firefox Suggestion has been clicked |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
