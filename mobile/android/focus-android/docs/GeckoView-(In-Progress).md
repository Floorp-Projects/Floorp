# Running GeckoView in a debug environment

Currently there are two ways to use GeckoView in development.

1. You can navigate to `focus:test` and flip the toggle to switch to the new engine
2. In [`Config.kt`](https://github.com/mozilla-mobile/focus-android/blob/master/app/src/debug/java/org/mozilla/focus/web/Config.kt) you can set `DEFAULT_NEW_RENDERER` to `true`.
