# [Android Components](../../../README.md) > Feature > Media

Feature component for website media related features.

## Usage

Add the push service for controlling the media session:

```kotlin
class MediaSessionService(
    override val store: BrowserStore,
    override val crashReporter: CrashReporting
) : AbstractMediaSessionService()
```

Expose the service in the `AndroidManifest.xml`:
```xml
<service android:name=".media.MediaSessionService"
    android:foregroundServiceType="mediaPlayback"
    android:exported="false" />
```

The `AbstractMediaSessionService` also requires extra permissions needed to post notification updates on media changes:
- `android.permission.POST_NOTIFICATIONS`
- `android.permission.FOREGROUND_SERVICE`
- `android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK`

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-media:{latest-version}"
```

### Notification: Recording devices

`RecordingDevicesMiddleware` can be used to show an ongoing notification when a recording device (camera,
microphone) is used by web content. Notifications will be shown in the "Media" notification channel.

This feature should only be initialized once globally:

```kotlin
BrowserStore(
  middleware = listOf(
    RecordingDevicesMiddleware(applicationContext, notificationsDelegate)
  )
)
```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item            |  Description                              |
|--------|-----------------|-------------------------------------------|
| PLAY   | state           | Media started playing.                    |
| PAUSE  | state           | Media playback was paused.                |
| STOP   | state           | Media playback has ended.                 |
| PLAY   | notification    | Play action of notification was invoked   |
| PAUSE  | notification    | Pause action of notification was invoked  |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
