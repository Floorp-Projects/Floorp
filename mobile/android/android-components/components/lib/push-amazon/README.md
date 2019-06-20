# [Android Components](../../../README.md) > Libraries > Push-Amazon

A [concept-push](../../concept/push/README.md) implementation using [Amazon Device Messaging](https://developer.amazon.com/docs/adm/overview.html) (ADM).

This implementation of `concept-push` uses [Amazon Device Messaging](https://developer.amazon.com/docs/adm/overview.html). It can be used by supported-Amazon Android devices.

## Usage

Add the push service for providing the encrypted messages:

```kotlin
class AmazonPush : AbstractAmazonPushService()
```

TBD

The service can be started/stopped directly if required:
```kotlin
val service = AmazonPush()

serivce.start()
serivce.stop()
```

See `feature-push` for more details on how to use the service with Autopush.

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:lib-push-amazon:{latest-version}"
```

### Adding ADM Support

TBD

See the [concept-push documentation](../../concept/push/README.md) for generic examples of using the API of components implementing `concept-push`.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
