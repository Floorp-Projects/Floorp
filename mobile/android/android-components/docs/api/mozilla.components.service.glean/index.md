[android-components](../index.md) / [mozilla.components.service.glean](./index.md)

## Package mozilla.components.service.glean

### Types

| Name | Summary |
|---|---|
| [Glean](-glean/index.md) | `object Glean`<br>In contrast with other glean-ac classes (i.e. Configuration), we can't use typealias to export mozilla.telemetry.glean.Glean, as we need to provide a different default [Configuration](../mozilla.components.service.glean.config/-configuration/index.md). Moreover, we can't simply delegate other methods or inherit, since that doesn't work for `object` in Kotlin. |
