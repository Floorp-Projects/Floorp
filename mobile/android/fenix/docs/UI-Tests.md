# UI Tests

## Espresso Instrumented UI Tests in Android

UI tests, including Espresso for Android development, play a crucial role in ensuring quality, reliability, and performance of Firefox and Focus for Android. They facilitate user interaction by simulating real-world scenarios, thereby catching real issues on devices that might not be apparent during manual testing or development. By integrating UI tests into the development process, developers can ensure their applications meet user expectations and provide a high-quality user experience across different devices and Android versions. By writing and executing these tests, developers can catch and fix issues earlier in the development cycle, significantly improving the quality and reliability of Firefox. This approach not only enhances the user experience but also saves time and resources by identifying and resolving problems before Firefox reaches end-users.

This page documents how to write and run UI tests locally in Android Studio.

## Writing UI Tests

A simple Espresso UI test looks like:

```Kotlin
@Test
fun displaySaysHello() {
    onView(withId(R.id.name_field)).perform(typeText("Firefox"))
    onView(withId(R.id.display_button)).perform(click())
    onView(withText("Hello Firefox!")).check(matches(isDisplayed()))
}
```

## Structuring Your Test

1. **Create a Test Class**: Start by creating a new test class which should extend a base test setup class (`TestSetup`)
2. **Use Test Rules**: Utilize test rules to manage the lifecycle of the test activity. Examples include `HomeActivityIntentTestRule`
3. **Define Test Methods**: Write test methods to cover the functionality you want to test. Each method should represent a specific test case.
4. **Leverage the Robot Pattern:** Use the Robot Pattern to encapsulate UI interactions and assertions. This pattern involves creating a separate class for each screen or feature under test. Each robo class encapsulates the actions and assertions related to that specific screen or feature.


## Writing Test Cases

1. **Set Up Your Test**: Before running your test, set up the necessary conditions. This might involve navigating to a specific screen, or preparing Firefox state.
2. **Perform Actions**: Use the robot classes to perform actions on the UI. This could involve entering text, clicking buttons, or navigation throughout Firefox.
3. **Verify Outcomes**: After performing actions, verify the expected outcomes. This could involve checking if a UI element is displayed, or if text matches.
4. **Clean Up**: After your test, clean up any changes you made to the app state. This might involve clearing history, or resetting to initial application state.

## Running UI Tests

To run UI tests, you can either use the built-in test runner in Android Studio or run them from the command line using Gradle commands.

## Gradle

To run UI tests under `androidTest` using Gradle, you can utilize the `connectedFenixDebugAndroidTest` task, which is specifically tailored for running instrumented tests on the `FenixDebug` build variant. This task is part of the Android Gradle Plugin’s suite of commands that allow for the execution of tests directly from the command line. For instance, to run a specific test class, such as `org.mozilla.fenix.ui.ComposeSearchTest`, you would navigate to Fenix’s root directory in the terminal and execute the following command:

```
./gradlew connectedFenixDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=org.mozilla.fenix.ui.ComposeSearchTest
```

This command targets the `FenixDebug` build variant and runs only the tests within the `ComposeSearchTest` class.

Similarly, to run a single test within the `ComposeSearchTest` class, you can append the test method name to the class using the `#` symbol. This syntax allows you to specify a particular test method to execute.

For example, if you have a test method named `testSearchFunctionality` within `ComposeSearchTest` class, you would modify the above example to look like `org.mozilla.fenix.ui.ComposeSearchTest#testSearchFunctionality`

Lastly, to run all tests in a package simply replace `.class` property with `.package` and specify a package name. This approach allows you to target a specific package for test execution, ensuring only the tests within that package are run.

The results of these tests can be found in the `app/build/reports` directory, where you will find both HTML and XML reports detailing the outcomes of your test execution.

## Android Studio

To run UI tests directly in Android Studio, you can follow a straightforward process that leverages the IDE’s built-in test runner. This method allows you to execute tests directly from the Project window, providing a convenient way to run individual tests or groups of tests without leaving the development environment.

1. **Open the Project Window**: On the left side of Android Studio, you’ll find the Project window. This window displays the structure of Fenix, including the `src/androidTest/java/org.mozilla.fenix/ui` directory where UI tests are located.
2. **Navigate to the target test Class**: In the Project window, navigate to the `src/androidTest/java/org.mozilla.fenix.ui` directory. Here you will find all test classes.
3. **Run a Single Test**:  To run a single test, right-click on the test class file you wish to run. From the context menu that appears, select “Run ‘ClassNameTest’”. Replace `ClassNameTest` with the actual name of your test class. Android Studio will compile your app and run the test class on the selected device or emulator.
4. **Run All Tests in a Class**: To run all tests within a specific test class, right-click on the class file and select “Run ‘ClassNameTest”. This will execute all test methods within that class.
5. **Run All Tests in a Package**: To run all tests within a package, right-click on the package name in the Project window and select “Run Tests”. This will execute all test classes within the selected package.
6. **View Test Results**: After running your tests, you can view the results in the Run window at the bottom of Android Studio. This window displays the outcome of each test, including passed, failed, and skipped tests. You can click on individual tests to see more details about their execution.

## Testing Resources

* [Espresso](https://developer.android.com/training/testing/espresso)
* [Espresso Basics](https://developer.android.com/training/testing/espresso/basics)
