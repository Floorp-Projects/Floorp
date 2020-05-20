---
layout: default
title: Getting Started with GeckoView
nav_order: 2 
summary: How to use GeckoView in your Android app.
tags: [GeckoView,Gecko,mozilla,android,WebView,mobile,mozilla-central,setup,quick start]
---

_Building a browser? Check out [Android Components](https://mozilla-mobile.github.io/android-components/), our collection of ready-to-use support libraries!_

The following article is a brief guide to embedding GeckoView in an app. For a more in depth tutorial on getting started with GeckoView please read the article we have published on [raywenderlich.com](https://www.raywenderlich.com/1381698-android-tutorial-for-geckoview-getting-started). 

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

## Configure Gradle

You need to add or edit four stanzas inside your module's `build.gradle` file.

**1. Set the GeckoView version**

_Like Firefox, GeckoView has three release channels: Stable, Beta, and Nightly. Browse the [Maven Repository](https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/) to see currently available builds._

```groovy
ext {
    geckoviewChannel = "nightly"
    geckoviewVersion = "70.0.20190712095934"
}
```

**2. Add Mozilla's Maven repository**
```groovy
repositories {
    maven {
        url "https://maven.mozilla.org/maven2/"
    }
}
```

**3. Java 8 required support** 

As GeckoView uses some Java 8 APIs, it requires these compatibility flags:

```groovy
compileOptions {
    sourceCompatibility JavaVersion.VERSION_1_8
    targetCompatibility JavaVersion.VERSION_1_8
}
```

**4. Add GeckoView Implementations**

```groovy
dependencies {
    // ...
  implementation "org.mozilla.geckoview:geckoview-${geckoviewChannel}:${geckoviewVersion}"   
}
```

## Add GeckoView to a Layout

Inside a layout `.xml` file, add the following:

```xml
<org.mozilla.geckoview.GeckoView
    android:id="@+id/geckoview"
    android:layout_width="fill_parent"
    android:layout_height="fill_parent" />
```

## Initialize GeckoView in an Activity

**1. Import the GeckoView classes inside an Activity:**

```java
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoView;
```

**2. In that activity's <code>onCreate</code> function, add the following:**

```java
GeckoView view = findViewById(R.id.geckoview);
GeckoSession session = new GeckoSession();
GeckoRuntime runtime = GeckoRuntime.create(this);

session.open(runtime);
view.setSession(session);
session.loadUri("about:buildconfig"); // Or any other URL...
```

## You're done!

Your application should now load and display a webpage inside of GeckoView.

To learn more about GeckoView's capabilities, review GeckoView's [JavaDoc](https://mozilla.github.io/geckoview/javadoc/mozilla-central/) or the [reference application](https://searchfox.org/mozilla-central/source/mobile/android/geckoview_example).
