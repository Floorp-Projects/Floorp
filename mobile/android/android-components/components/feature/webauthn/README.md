# [Android Components](../../../README.md) > Feature > WebAuthn

A feature that provides WebAuthn functionality for supported engines.

## Usage

Add the feature to the Activity/Fragment:

```kotlin
val webAuthnFeature = WebAuthnFeature(
    engine = GeckoEngine,
    activity = requireActivity()
)
```

**Note:** If the feature is on the fragment, ensure that `onActivityResult` calls from the activity are forwarded to the fragment.

Allow the feature to consume the `onActivityResult` data:

```kotlin
override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
   webAuthFeature.onActiviyResult(requestCode, data, resultCode)
}
```

As with other features in Android Components, `WebAuthnFeature` implements `LifecycleAwareFeature`, so it's recommended to use `ViewBoundFeatureWrapper` to handle the lifecycle events of the feature:

```kotlin
private val webAuthnFeature = ViewBoundFeatureWrapper<WebAuthnFeature>()

override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
    webAuthnFeature.set(
        feature = WebAuthnFeature(
            engine = GeckoEngine,
            activity = requireActivity()
        ),
        owner = this,
        view = view
    )
}

override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
    webAuthnFeature.onActivityResult(requestCode, data, resultCode) }
}
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-webauthn:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
