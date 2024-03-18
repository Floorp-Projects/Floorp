# [Android Components](../../../README.md) > Browser > Engine-System

[*Engine*](../../concept/engine/README.md) implementation based on the system's WebView.

## Usage

See [concept-engine](../../concept/engine/README.md) for a documentation of the abstract engine API this component implements.

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-engine-system:{latest-version}"
```

### Initializing

It is recommended t create only one `SystemEngine` instance per app.

```Kotlin
// Create default settings (optional) and enable tracking protection for all future sessions.
val defaultSettings = DefaultSettings().apply {
    trackingProtectionPolicy = EngineSession.TrackingProtectionPolicy.all()
}

// Create an engine instance to be used by other components.
val engine = SystemEngine(context, defaultSettings)
```

### Integration

Usually it is not needed to interact with the `Engine` component directly. The [browser-session](../session/README.md) component will take care of making the state accessible and link a `Session` to an `EngineSession` internally. The [feature-session](../../feature/session/README.md) component will provide "use cases" to perform actions like loading URLs and takes care of rendering the selected `Session` on an `EngineView`.

### View

`SystemEngineView` is the Gecko-based implementation of `EngineView` in order to render web content.

```XML
<mozilla.components.browser.engine.system.SystemEngineView
    android:id="@+id/engineView"
    android:layout_width="match_parent"
    android:layout_height="match_parent" />
```

`SystemEngineView ` can render any `SystemEngineSession` using the `render()` method.

```Kotlin
val engineSession = engine.createSession()
val engineView = view.findViewById<SystemEngineView>(R.id.engineView)
engineView.render(engineSession)
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
