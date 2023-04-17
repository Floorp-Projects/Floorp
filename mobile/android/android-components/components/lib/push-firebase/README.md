# [Android Components](../../../README.md) > Libraries > Push-Firebase

A [concept-push](../../concept/push/README.md) implementation using [Firebase Cloud Messaging](https://firebase.google.com/products/cloud-messaging/) (FCM).

This implementation of `concept-push` uses [Firebase Cloud Messaging](https://firebase.google.com/products/cloud-messaging/). It can be used by Android devices that are supposed by Google Play Services.

## Usage

Add the push service for providing the encrypted messages:

```kotlin
class FirebasePush : AbstractFirebasePushService()
```

Expose the service in the `AndroidManifest.xml`:
```xml
<service android:name=".push.FirebasePush">
    <intent-filter>
        <action android:name="com.google.firebase.MESSAGING_EVENT" />
    </intent-filter>
</service>
```

The service can be started/stopped directly if required:
```kotlin
val service = FirebasePush()

serivce.start()
serivce.stop()
```

See `feature-push` for more details on how to use the service with Autopush.

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:lib-push-firebase:{latest-version}"
```

### Adding Firebase Support

Extend `AbstractFirebasePushService` with your own class:
```kotlin
class FirebasePush : AbstractFirebasePushService()
```

Place your keys file (`google-services.json`) for FCM in the app module of the project.

Optionally, add meta tags to your `AndroidManifest.xml` to disable the push service from automatically starting.

See the [concept-push documentation](../../concept/push/README.md) for generic examples of using the API of components implementing `concept-push`.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
