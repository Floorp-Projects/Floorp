# [Android Components](../../../README.md) > Feature > Push

A component that implements push notifications with a supported push service.

## Usage

Add a supported push service for providing the encrypted messages (for example Firebase Cloud Messaging via `lib-push-firebase`):
```kotlin
class FirebasePush : AbstractFirebasePushService()
```

Create a push configuration with the project info and also place the required service's API keys in the project directory:

```kotlin
PushConfig(
  senderId = "push-test-f408f",
  serverHost = "push.service.mozilla.com",
  serviceType = ServiceType.FCM,
  protocol = Protocol.HTTPS
)
```

We can then start the AutoPushFeature to get the subscription info and decrypted push message:
```kolin
val service = FirebasePush()

val feature = AutoPushFeature(
  context = context,
  service = pushService,
  config = config
)

// To start the feature and the service.
feature.initialize()

// To stop the feature and the service.
feature.shutdown()

// To receive the subscription info for all the subscription changes.
feature.registerForSubscriptions(object : PushSubscriptionObserver {
  override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
    // handle subscription info here.
  }
}, lifecycle, false)

// Force request all subscription information.
feature.subscribeAll()

// To receive a message for a specific push message type.
feature.registerForPushMessages(PushType.Services, object: Bus.Observer<PushType, String> {
  override fun onEvent(type: PushType, message: String) {
    // Handle decrypted message here.
  }
}, lifecycle, false)
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-push:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
