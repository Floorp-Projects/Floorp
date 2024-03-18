# [Android Components](../../../README.md) > Feature > Find In Page

A feature that provides [Find in Page functionality](https://support.mozilla.org/en-US/kb/search-contents-current-page-text-or-links).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-findinpage:{latest-version}"
```

### Adding feature to application

To use this feature you have to do two things:

**1. Add the `FindInPageBar` widget to you layout:**

```xml
<mozilla.components.feature.findinpage.view.FindInPageBar
        android:id="@+id/find_in_page"
        android:layout_width="match_parent"
        android:background="#FFFFFFFF"
        android:elevation="10dp"
        android:layout_height="56dp"
        android:padding="4dp" />
```

These are the properties that you can customize of this widget.
```xml
<attr name="findInPageQueryTextColor" format="reference|color"/>
<attr name="findInPageQueryHintTextColor" format="reference|color"/>
<attr name="findInPageQueryTextSize" format="dimension"/>
<attr name="findInPageResultCountTextColor" format="reference|color"/>
<attr name="findInPageResultCountTextSize" format="dimension"/>
<attr name="findInPageButtonsTint" format="reference|color"/>
<attr name="findInPageNoMatchesTextColor" format="reference|color"/>
```

**2. Add the `FindInPageFeature` to your activity/fragment:**

```kotlin
val findInPageBar = layout.findViewById<FindInPageBar>(R.id.find_in_page)

val findInPageFeature = FindInPageFeature(
    sessionManager,
    findInPageView
) {
    // Optional: Handle clicking of "close" button.
}

lifecycle.addObservers(findInPageFeature)

// To show "Find in Page" results for a `Session`:
findInPageFeature.bind(session)
```

🦊 A practical example of using feature find in page can be found in [Sample Browser](https://github.com/mozilla-mobile/android-components/tree/main/samples/browser).

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item     | Extras        | Description                                         |
|--------|----------|---------------|-----------------------------------------------------|
| CLICK  | previous |               | The user clicked the previous result button.        |
| CLICK  | next     |               | The user clicked the next result button.            |
| CLICK  | close    |               | The user clicked the close button.                  |
| COMMIT | input    | `inputExtras` | The user committed a query to be found on the page. |


#### `inputExtras`

| Key   | Type   | Value                       |
|-------|--------|-----------------------------|
| value | String | The query that was searched |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
