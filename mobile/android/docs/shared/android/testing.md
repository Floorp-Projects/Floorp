# Testing
*This document is intended to explain which testing styles and technologies the Android teams at Mozilla use and why. It should be accessible to developers new to testing on Android.*

*Presently this document describes testing on Firefox for Fire TV: it should grow to include other applications.*

The primary problem we try to solve with automated testing is preventing unintended changes, which often lead to bugs or poor user experiences. This problem is most common -- thus we benefit the most from thorough testing -- when developers are modifying complex or unfamiliar code.

## Quick primer on automated testing
Automated tests are typically broken up into the following categories:
- End-to-End tests
  - Black box tests that act like users: they click the screen and assert what is displayed, typically following common user scenarios
  - Notable for being **the only test of the full system.** Tend to **run slowly**, **be fragile**, and **be difficult to debug**
- Integration tests
  - Tests comprising of multiple parts of the system: they can modify and assert internal state
  - Compromise between end-to-end and unit tests. Tend to **run fairly quickly** since, unlike UI tests, they don't need to wait for UI changes
- Unit tests
  - Tiny tests that test one single bit of functionality; they typically don't overlap
  - When written with non-overlapping assertions across tests, notable for **clearly identifying a single piece of broken functionality in a system.** Tend to **run quickly**.

Due to trade-offs of speed, correctness, fragility, and ability to debug, it's recommended to have a significant number of unit tests, a small number of integration tests, and an even smaller number of end-to-end tests. For more details, research the Test Pyramid.

## Automated testing on Android
Android tests are broken into two major categories:
- On-device testing
  - Runs on Android hardware or emulator
  - Typically used for **end-to-end and integration tests**
  - Accurate for real world users but slow and fragile due to coordinating two devices, low power hardware
  - Located in a sourceset like, `<proj>/app/src/androidTest`
- JVM testing
  - Runs on your development machine
  - Typically used for **unit tests**
  - Runs very quickly
  - Located in a sourceset like, `<proj>/app/src/test`

In all categories, by default tests are declared using the JUnit APIs (see below).

Note: Google [is converging the test APIs][Frictionless Android Testing] for on-device and off-device testing so eventually you can write once and run anywhere.

### On-device testing
Users run your code on Android devices so testing on these devices is **the most accurate form of testing**. However, between copying code and execution times, running tests on Android devices or emulators is **significantly slower** than running code on your development machine's JVM, especially when the test suites get large. As such, **on-device testing should generally only be used when:**
- Running end-to-end tests or some integration tests
- Reproducing the behavior on device is critical
- The tests cannot be written accurately for the JVM (e.g. UI)

See [Physical Device Testing](device_testing.md) for how to leverage available physical devices.

#### Espresso and UI Automator: core UI testing libraries
[Espresso] and [UI Automator] are Google's core UI testing libraries on Android: they provide APIs to select views, perform actions on them, and assert state without reaching into implementation details.

**Espresso is *significantly* more reliable** than UI Automator because it provides **an API for `IdlingResource`s:** these APIs wait for some system to be idle before executing the next Espresso action. For example, the built-in `IdlingResource` will wait for the UI thread to be idle, allowing the test to be robust during asynchronous actions on the UI thread, animations, etc. You can write custom `IdlingResource`s.

**UI Automator can perform actions outside the scope of the application,** on the entire system, unlike Espresso. You can use this if you need to close and reopen the app, interact with the notification tray, etc.

These libraries can be used in the same test. However, due to the improved reliability, **Espresso is preferred for most interactions.** To use these libraries, add them to gradle, import them, and call them directly.

See [this FFTV example of Espresso and UI Automator working together][espresso example].

#### Robot pattern: UI test architecture
UI tests are notoriously difficult to write: the code is often duplicated across tests, written imperatively, and needs frequent updates to keep up with changing UI.

The Robot Pattern is an architecture that attempts to address these problems: its **key feature is separating the "what" from the "how", making the tests declarative.** A Robot pattern test may look like:
```
navigationOverlay {
    assertCanGoBack()
    goBack()
    assertCanNotGoBack()

}.enterUrlAndEnterToBrowser("https://mozilla.org") {
    assertBodyContent("Welcome to mozilla.org!")
}
```

For more on the Robot pattern including learning resources, see [the Firefox TV Architecture Decision Record](https://github.com/mozilla-mobile/firefox-tv/blob/master/docs/architecture/adr-0002-robot-pattern.md). See [this FFTV example of the Robot pattern][robot example].

### JVM Testing
**Most tests should be written to run on your development machine because of the speed benefits.** Typically these tests are written as "unit tests" where each test will test a non-overlapping set of functionality so, if they fail, you can precisely pinpoint what is broken.

These tests can be written effectively with standard JUnit APIs.

Out-of-the-box accessing the Android framework APIs is not supported, however. You can use [Robolectric][] to "shadow" the Android framework (see below).

#### Robolectric: Android API shadows
The Android APIs are not available on your development machine and will cause your tests to throw exceptions when used. However, **[Robolectric] provides "shadows" that mimic the Android APIs on your development machine,** allowing you to write tests with working Android APIs.

While the shadows are good enough to prevent crashes, sometimes they are incomplete: for example, the shadow for `Bitmap` may not return pixel data which would cause Bitmap tests to fail. In these cases, you can fix the issue by [writing custom shadows](http://robolectric.org/extending/), interacting with a mock, or testing on device instead.

**Robolectric tests take longer to run than non-Robolectric tests** because they run additional setup code (e.g. initializing the `Application`): we are mildly concerned this may be non-negligible in very large test suites.

To use Robolectric, annotate your test class with `@RunWith(RobolectricTestRunner::class)` (in Kotlin; [see this example][robolectric example]).

Note: Robolectric can also be used for JVM-based UI testing but we don't have much experience with that yet.

### Cross-platform testing technologies
Technologies that are used for both on-device testing and tests on your development machine are...

#### JUnit: testing framework
[JUnit] is the **go-to unit testing framework on the JVM.** It provides ways to define tests and test suites and provides assertion methods like `assertEquals(expectedValue, actualValue)`.

By default, all test suites on Android are defined using JUnit 4. The latest version is JUnit 5.

#### Mockito: mocking framework
[Mockito] is the **go-to mocking framework on the JVM:** it includes mock, spy, and call count verification support. Mocks are used to implement custom functionality for classes without having to extend the classes because extending often requires a lot of boilerplate and fragile code.

Mockito cannot be used to mock static methods. In these cases, you can also include [PowerMock] but know that mocking static methods is arguably considered a poor practice.

See [this FFTV example of Mockito][mockito example].

Mockito does not interact well with Kotlin so we are considering alternatives, such as:

#### MockK: alternate mocking framework
[MockK] is a popular **mocking framework for Kotlin:** it includes mock, spy, and call count verification support that better aligns with Kotlin APIs. It can **mock static methods, extension functions, and objects**. We have documentation written for [using MockK](https://notwoods.github.io/mockk-guidebook/) and [transitioning from Mockito](https://notwoods.github.io/mockk-guidebook/docs/mockito-migrate/).

See [this Fenix example of MockK][mockk example].

#### MockWebServer: mock network interactions
The network is unreliable so it should be avoided during testing. Instead, you can use **[MockWebServer] to provide your own data from "the network".** You access it by making your calls to the URL it returns. This lets it work with `WebView`s.

See [this FFTV example of MockWebServer][mockwebserver example].

## Best practices
This section is intended to be a non-exhaustive list of high-level guidelines (not rules!) when testing.

- Don't depend on external factors, like the network, to ensure your tests are always reliable
- For unit testing your UI, architect your application to separate your model from your UI, e.g. with architectures like MVP, MVVM, and MVI. Without consciously separating these parts of your application, it may be too difficult to test
- For UI testing, disable animations: Espresso's handling of them is mediocre
- In addition to testing standard stuff like business logic and UI state, consider testing:
  - Every "configuration" at least once: different screen sizes, different API levels, your A/B test experiments, l10n if you have different behavior in some locales
  - Telemetry
  - Accessibility
  - Performance (event duration, CPU use, memory use, APK size)

## Testing resources
Members of our team recommend, ["Working Effectively with Legacy Code"][legacy code] to learn how to introduce tests into an existing, untested project.

[Espresso]: https://developer.android.com/training/testing/espresso/
[UI Automator]: https://developer.android.com/training/testing/ui-automator

[Robolectric]: http://robolectric.org/

[JUnit]: https://junit.org/junit4/
[Mockito]: https://site.mockito.org/
[MockK]: https://mockk.io/
[PowerMock]: https://github.com/powermock/powermock
[MockWebServer]: https://github.com/square/okhttp/tree/master/mockwebserver

[legacy code]: https://www.goodreads.com/book/show/44919.Working_Effectively_with_Legacy_Code

[espresso example]: https://github.com/mozilla-mobile/firefox-tv/blob/f27a206cbee0e9bc7c9df9c4e93aae88198d211a/app/src/androidTest/java/org/mozilla/tv/firefox/ui/screenshots/SettingsTest.java#L51
[robot example]: https://github.com/mozilla-mobile/firefox-tv/blob/f27a206cbee0e9bc7c9df9c4e93aae88198d211a/app/src/androidTest/java/org/mozilla/tv/firefox/ui/BasicNavigationTest.kt#L50
[robolectric example]: https://github.com/mozilla-mobile/firefox-tv/blob/dd236b03560c3c438cb2c674f3c453f877e79bf5/app/src/test/java/org/mozilla/tv/firefox/ext/ContextTest.kt#L15
[mockito example]: https://github.com/mozilla-mobile/firefox-tv/blob/11b9eb7be32787ebf919884a74a15d07781b4640/app/src/test/java/org/mozilla/tv/firefox/ext/AssetManagerTest.kt#L30
[mockk example]: https://github.com/mozilla-mobile/fenix/blob/6bde0378a223b74f9c22e5e664a250ff7534dc11/app/src/test/java/org/mozilla/fenix/ext/ContextTest.kt#L104
[mockwebserver example]: https://github.com/mozilla-mobile/firefox-tv/blob/4e8c9923dd6016ece5830d1eaaf58c1761d8e57f/app/src/androidTest/java/org/mozilla/tv/firefox/ui/IWebViewExecuteJavascriptTest.kt#L41

[Frictionless Android Testing]: https://www.youtube.com/watch?v=wYMIadv9iF8
