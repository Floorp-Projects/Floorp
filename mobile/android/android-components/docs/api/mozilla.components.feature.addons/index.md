[android-components](../index.md) / [mozilla.components.feature.addons](./index.md)

## Package mozilla.components.feature.addons

### Types

| Name | Summary |
|---|---|
| [Addon](-addon/index.md) | `data class Addon`<br>Represents an add-on based on the AMO store: https://addons.mozilla.org/en-US/firefox/ |
| [AddonManager](-addon-manager/index.md) | `class AddonManager`<br>Provides access to installed and recommended [Addon](-addon/index.md)s and manages their states. |
| [AddonsProvider](-addons-provider/index.md) | `interface AddonsProvider`<br>A contract that indicate how an add-on provider must behave. |

### Exceptions

| Name | Summary |
|---|---|
| [AddonManagerException](-addon-manager-exception/index.md) | `class AddonManagerException : `[`Exception`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-exception/index.html)<br>Wraps exceptions thrown by either the initialization process or an [AddonsProvider](-addons-provider/index.md). |
