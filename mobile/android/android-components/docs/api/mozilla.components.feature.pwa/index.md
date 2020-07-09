[android-components](../index.md) / [mozilla.components.feature.pwa](./index.md)

## Package mozilla.components.feature.pwa

### Types

| Name | Summary |
|---|---|
| [ManifestStorage](-manifest-storage/index.md) | `class ManifestStorage`<br>Disk storage for [WebAppManifest](../mozilla.components.concept.engine.manifest/-web-app-manifest/index.md). Other components use this class to reload a saved manifest. |
| [ProgressiveWebAppFacts](-progressive-web-app-facts/index.md) | `class ProgressiveWebAppFacts`<br>Facts emitted for telemetry related to [PwaFeature](#) |
| [WebAppInterceptor](-web-app-interceptor/index.md) | `class WebAppInterceptor : `[`RequestInterceptor`](../mozilla.components.concept.engine.request/-request-interceptor/index.md)<br>This feature will intercept requests and reopen them in the corresponding installed PWA, if any. |
| [WebAppLauncherActivity](-web-app-launcher-activity/index.md) | `class WebAppLauncherActivity : AppCompatActivity`<br>This activity is launched by Web App shortcuts on the home screen. |
| [WebAppShortcutManager](-web-app-shortcut-manager/index.md) | `class WebAppShortcutManager`<br>Helper to manage pinned shortcuts for websites. |
| [WebAppUseCases](-web-app-use-cases/index.md) | `class WebAppUseCases`<br>These use cases allow for adding a web app or web site to the homescreen. |

### Properties

| Name | Summary |
|---|---|
| [SHORTCUT_CATEGORY](-s-h-o-r-t-c-u-t_-c-a-t-e-g-o-r-y.md) | `const val SHORTCUT_CATEGORY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
