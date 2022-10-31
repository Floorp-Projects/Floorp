# [Android Components](../../../README.md) > Feature > Reader View

 A component wrapping/providing a Reader View WebExtension.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-readerview:{latest-version}"
```

### Integration

Initializing the feature:

```kotlin
val readerViewFeature = ReaderViewFeature(
    context,
    engine,
    sessionManager,
    onReaderViewAvailableChange = {
    	// e.g. readerViewToolbarActionVisible = it
    }
)

```

Showing and hiding Reader View:

```kotlin
readerViewFeature.showReaderView()
readerViewFeature.hideReaderView()
```

Showing and hiding the Reader View appearance UI (to adjust font size, font type and color scheme). Note that changes to the appearance settings are automatically persisted as user preferences.

```kotlin
readerViewFeature.showAppearanceControls()
readerViewFeature.hideAppearanceControls()
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
