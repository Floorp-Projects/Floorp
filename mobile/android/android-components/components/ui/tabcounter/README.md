# [Android Components](../../../README.md) > UI > Tabcounter

A button that shows the current tab count and can animate state changes.

## Usage

Create a tab counter in XML:

```xml
<mozilla.components.ui.tabcounter.TabCounter
  android:layout_width="wrap_content"
  android:layout_height="wrap_content"
  mozac:tabCounterTintColor="@color/primary" />
```

Styleable attributes can be set on your theme as well:

```xml
<style name="AppTheme">
    ...
    <item name="tabCounterTintColor">#FFFFFF</item>
</style>
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:ui-tabcounter:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
