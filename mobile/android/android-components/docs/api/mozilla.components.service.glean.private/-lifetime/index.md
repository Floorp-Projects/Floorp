[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [Lifetime](./index.md)

# Lifetime

`enum class Lifetime` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/CommonMetricData.kt#L13)

Enumeration of different metric lifetimes.

### Enum Values

| Name | Summary |
|---|---|
| [Ping](-ping.md) | The metric is reset with each sent ping |
| [Application](-application.md) | The metric is reset on application restart |
| [User](-user.md) | The metric is reset with each user profile |
