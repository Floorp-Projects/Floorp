# [Android Components](../../../README.md) > Feature > Media

Feature component for website media related features.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-media:{latest-version}"
```

### Notification: Recording devices

`RecordingDevicesNotificationFeature` can be used to show an ongoing notification when a recording device (camera,
microphone) is used by web content. Notifications will be shown in the "Media" notification channel.

This feature should only be initialized once globally:

```kotlin
RecordingDevicesNotificationFeature(applicationContext, sessionManager)
    .enable()
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
