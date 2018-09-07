# [Android Components](../../../README.md) > Concept > Engine

The `concept-engine` component contains interfaces and abstract classes that hide the actual browser engine implementation from other components needing access to the browser engine.

There are implementations for [WebView](https://developer.android.com/reference/android/webkit/WebView) and multiple release channels of [GeckoView](https://wiki.mozilla.org/Mobile/GeckoView) available.

Other components and apps only referencing `concept-engine` makes it possible to:

* Build components that work independently of the engine being used.
* Build apps that can work with multiple engines (Compile-time or Run-time).
* Build apps that can be build against different GeckoView release channels (Nightly/Beta/Release).

## Usage

### Setting up the dependency

Use gradle to download the library from JCenter:

```Groovy
implementation "org.mozilla.components:engine:{latest-version}
```

### Integration

Usually it is not needed to interact with the `Engine` component directly. The [browser-session](../../browser/session/README.md) component will take care of making the state accessible and link a `Session` to an `EngineSession` internally. The [feature-session](../../feature/session/README.md) component will provide "use cases" to perform actions like loading URLs and takes care of rendering the selected `Session` on an `EngineView`.
``
### Observing changes

Every `EngineSession` can be observed for changes by registering an `EngineSession.Observer` instance.

```Kotlin
engineSession.register(object : EngineSession.Observer {
    onLocationChange(url: String) {
        // This session is pointing to a different URL now.
    }
})
```

`EngineSession.Observer` provides empty default implementation of every method so that only the needed ones need to be overridden. See the API reference of the current version to see all available methods.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
