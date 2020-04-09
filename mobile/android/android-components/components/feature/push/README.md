# [Android Components](../../../README.md) > Feature > Push

A component that implements push notifications with a supported push service.

## Usage

Add a supported push service for providing the encrypted messages (for example, Firebase Cloud Messaging via `lib-push-firebase`):
```kotlin
class FirebasePush : AbstractFirebasePushService()
```

Create a push configuration with the project info and also place the required service's API keys in the project directory:

```kotlin
PushConfig(
  senderId = "push-test-f408f",
  serverHost = "updates.push.services.mozilla.com",
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
  config = pushConfig
)

// To start the feature and the service.
feature.initialize()

// To stop the feature and the service.
feature.shutdown()

// To receive the subscription info for all the subscription changes.
feature.register(object : AutoPushFeature.Observer {
  override fun onSubscriptionChanged(scope: PushScope) {
    // Handle subscription info here.
  }
})

// Subscribe for a unique scope (identifier).
feature.subscribe("push_subscription_scope_id")

// To receive messages:
feature.register(object : AutoPushFeature.Observer {
  override fun onMessageReceived(scope: String, message: ByteArray?) {
    // Handle decrypted message here.
  }
})
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-push:{latest-version}"
```

## Implementation Notes

Why do we need to verify connections, and what happens when we do?
- Various services may need to communicate with us via push messages. Examples: FxA events (send tab, etc), WebPush (a web app receives a push message from its server).
- To send these push messages, services (FxA, random internet servers talking to their web apps) post an HTTP request to a "push endpoint" maintained by [Mozilla's Autopush service][0]. This push endpoint is specific to its recipient - so one instance of an app may have many endpoints associated with it: one for the current FxA device, a few for web apps, etc.
- Important point here: servers (FxA, services behind web apps, etc.) need to be told about subscription info we get from Autopush.
- Here is where things start to get complicated: client (us) and server (Autopush) may disagree on which channels are associated with the current UAID (remember: our subscriptions are per-channel). Channels may expire (TTL'd) or may be deleted by some server's Cron job if they're unused. For example, if this happens, services that use this subscription info (e.g. FxA servers) to communication with their clients (FxA devices) will fail to deliver push messages.
- So the client needs to be able to find out that this is the case, re-create channel subscriptions on Autopush, and update any dependent services with new subscription info (e.g. update the FxA device record for `PushType.Services`, or notify the JS code with a `pushsubscriptionchanged` event for WebPush).
- The Autopush side of this is `verify_connection` API - we're expected to call this periodically, and that library will compare channel registrations that the server knows about vs those that the client knows about.
- If those are misaligned, we need to re-register affected (or, all?) channels, and notify related services so that they may update their own server-side records.
- For FxA, this means that we need to have an instance of the rust FirefoxAccount object around in order to call `setDevicePushSubscriptionAsync` once we re-generate our push subscription.
- For consumers such as Fenix, easiest way to access that method is via an `account manager`.
- However, neither account object itself, nor the account manager, aren't available from within a Worker. It's possible to "re-hydrate" (instantiate rust object from the locally persisted state) a FirefoxAccount instance, but that's a separate can of worms, and needs to be carefully considered.
- Similarly for WebPush (in the future), we will need to have Gecko around in order to fire `pushsubscriptionchanged` javascript events.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/

[0]: https://github.com/mozilla-services/autopush