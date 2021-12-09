.. -*- Mode: rst; fill-column: 80; -*-

Getting Started with GeckoView
######################################

How to use GeckoView in your Android app.

*Building a browser? Check out* `Android Components <https://mozilla-mobile.github.io/android-components/>`_, *our collection of ready-to-use support libraries!*

The following article is a brief guide to embedding GeckoView in an app. For a more in depth tutorial on getting started with GeckoView please read the article we have published on `raywenderlich.com <https://www.raywenderlich.com/1381698-android-tutorial-for-geckoview-getting-started>`_. 

.. contents:: :local:

Configure Gradle
=================

You need to add or edit four stanzas inside your module's ``build.gradle`` file.

**1. Set the GeckoView version**

*Like Firefox, GeckoView has three release channels: Stable, Beta, and Nightly. Browse the* `Maven Repository <https://maven.mozilla.org/?prefix=maven2/org/mozilla/geckoview/>`_ *to see currently available builds.*

.. code-block:: groovy 

    ext {
        geckoviewChannel = "nightly"
        geckoviewVersion = "70.0.20190712095934"
    }


**2. Add Mozilla's Maven repository**

.. code-block:: groovy 

    repositories {
        maven {
            url "https://maven.mozilla.org/maven2/"
        }
    }


**3. Java 8 required support** 

As GeckoView uses some Java 8 APIs, it requires these compatibility flags:

.. code-block:: groovy 

    compileOptions {
        sourceCompatibility JavaVersion.VERSION_1_8
        targetCompatibility JavaVersion.VERSION_1_8
    }

**4. Add GeckoView Implementations**

.. code-block:: groovy 

    dependencies {
        // ...
      implementation "org.mozilla.geckoview:geckoview-${geckoviewChannel}:${geckoviewVersion}"   
    }

Add GeckoView to a Layout
==========================

Inside a layout ``.xml`` file, add the following:

.. code-block:: xml 

    <org.mozilla.geckoview.GeckoView
        xmlns:android="http://schemas.android.com/apk/res/android"
        android:id="@+id/geckoview"
        android:layout_width="fill_parent"
        android:layout_height="fill_parent" />

Initialize GeckoView in an Activity
====================================

**1. Import the GeckoView classes inside an Activity:**

.. code-block:: java 

    import org.mozilla.geckoview.GeckoRuntime;
    import org.mozilla.geckoview.GeckoSession;
    import org.mozilla.geckoview.GeckoView;

**2. In that activity's** ``onCreate`` **function, add the following:**

.. code-block:: java 

    GeckoView view = findViewById(R.id.geckoview);
    GeckoSession session = new GeckoSession();
    GeckoRuntime runtime = GeckoRuntime.create(this);

    session.open(runtime);
    view.setSession(session);
    session.loadUri("about:buildconfig"); // Or any other URL...

You're done!
==============

Your application should now load and display a webpage inside of GeckoView.

To learn more about GeckoView's capabilities, review GeckoView's `JavaDoc <https://mozilla.github.io/geckoview/javadoc/mozilla-central/>`_ or the `reference application <https://searchfox.org/mozilla-central/source/mobile/android/geckoview_example>`_.
