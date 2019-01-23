# [Android Components](../../../README.md) > Feature > Find In Page

A feature that provides an UI widget for [find in page functionality](https://support.mozilla.org/en-US/kb/search-contents-current-page-text-or-links).

## Usage

### Setting up the dependency
Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-findinpage:{latest-version}"
```

### Adding feature find in page.
To use feature find in page you have to do two things:

**1. Add the `FindInPageView` widget to you layout.**

By default, it has a`GONE` visibility, and to make it visible just call `show()`.
```xml
        <mozilla.components.feature.findinpage.FindInPageView
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
```

**2. Start the `FindInPageFeature` in you activity/fragment.**

```kotlin
     val findInPageView = layout.findViewById<FindInPageView>(R.id.find_in_page)

     val  findInPageFeature = FindInPageFeature(
            sessionManager = sessionManager,
            findInPageView = findInPageView
        )

        lifecycle.addObservers( findInPageFeature)

       // To make it visible
       findInPageView.show()

       // Optional
       findInPageView.onCloseButtonPressListener = {
         // Do something.
        }

```

🦊 A practical example of using feature find in page can be found in [Sample Browser](https://github.com/mozilla-mobile/android-components/tree/master/samples/browser).

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
