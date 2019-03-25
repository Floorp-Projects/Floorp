---
layout: page
title: Working on unreleased component code in an app
permalink: /contributing/testing-components-inside-app
---

Sometimes it may be helpful to test component code you are working on inside an end-user app that is using this component.

## Avoid depending on apps living outside of the repository

Before trying to integrate a modified component into an *external* app try to re-create a test scenario **inside** the repository:

* Add **Unit tests**: Can you reproduce your scenario as a unit test with the help of [Robolectric](http://robolectric.org/)? Unit tests will create a reproducible scenario for every developer and can prevent regressions in the future.

* Add or modify a **sample app**: Especially for visual changes an app running on a test device may be needed. Can your scenario be reproduced with a new or existing sample app inside the repository? The sample app will make it easier for other developers to *see* and understand your scenario. And it will make it easier to re-test the same scenario with future versions of a component.

* Add a **user interface test** for a sample app: The UI test will prevent regressions in your sample app scenario and allows other developers to *replay* your scenario when changing component code.

## Using a local Maven repository to test local component code

Even if you are able to reproduce the scenario *inside* the repository you may still want to test your code with an external app consuming the component.  You can do that by *publishing* the component to your *local* Maven repository and configuring the app to pull the dependency from there.

#### Setup version number

In the *android-components* repository update the version number [in the configuration](https://github.com/mozilla-mobile/android-components/blob/master/.buildconfig.yml#L1) so that the app will be required to "download" this new version. Try to avoid picking a version number that was or will be officially released. Otherwise you may "pollute" your gradle cache with unofficial releases. To follow [the Maven convention](https://maven.apache.org/guides/getting-started/index.html#What_is_a_SNAPSHOT_version) you may want to use a `-SNAPSHOT` suffix.

#### Publish to local maven repository

After changing the components code you can publish your version to your local maven repository. You can either publish all components or just a specific ones:

```Bash
# Publish all components to your local Maven repository:
$ ./gradlew publishToMavenLocal

# Only publish a single component (ui-autocomplete):
$ ./gradlew ui-autocomplete:publishToMavenLocal
```

#### Using a component from the local maven repository

In your app project locate the `repositories` block inside your build.gradle and add `mavenLocal()` to the list:

```Groovy
repositories {
    mavenLocal()
    google()
    jcenter()
}
```

*Note*: There may be two `repositories` blocks in your root build.gradle. One for `buildscript` and one for `allprojects`. Make sure to add it to the `allprojects` block or to a build.gradle file inside a specific gradle module.

After that update the version number of your component dependency in your app's build files. Gradle will now resolve the dependency from your local maven repository and you can test your changes inside the app.

#### Iterating and caching

For every change in the android components repository you will need to run the `install` task to generate a new artifact and publish it to your local Maven repository.

When using a snapshot version (see above) in an app Gradle should ignore its cache and always try to use the latest version of the artifact.

To enforce using the latest version of a dependency you can try the `--refresh-dependencies` Gradle command line option or setting up a [ResolutionStrategy with a cache time of 0](https://docs.gradle.org/current/dsl/org.gradle.api.artifacts.ResolutionStrategy.html) in your app project.

## Automated Snapshots

Snapshots are build daily from the `master` branch and published on [snapshots.maven.mozilla.org](https://snapshots.maven.mozilla.org).
