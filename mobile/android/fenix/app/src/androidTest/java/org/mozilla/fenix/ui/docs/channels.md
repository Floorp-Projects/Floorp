# Espresso/UI Automator Tests on All Channels

When writing Espresso/UI Automator tests, by default, the tests are expected to run on all channels unless otherwise targeted. The provided code snippet below demonstrates a conditional check before running the tests on specific channels.

```
runWithCondition(
            // Returns the GeckoView channel set for the current version, if a feature is limited to Nightly or Beta.
            // Once this feature lands in RC we should remove the wrapper.
            activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.NIGHTLY ||
                activityIntentTestRule.activity.components.core.engine.version.releaseChannel == EngineReleaseChannel.BETA,
        )
```
The code uses the `runWithCondition()` function to determine the appropriate channel for the test. It checks if the current version's release channel is either Nightly or Beta using the `activityIntentTestRule.activity.components.core.engine.version.releaseChannel` property.

If the release channel is Nightly or Beta, the test is executed within the `runWithCondition()` block. However, once the feature under test lands in the Release Candidate (RC) channel, we suggest removing the wrapper and allowing the tests to run without any channel-specific condition.

This approach ensures that the tests are executed on all channels during the development and testing phase. However, when the feature stabilizes and reaches the RC channel, the conditional check can be removed to ensure the tests run consistently across all channels.

Please note that the actual implementation of the tests and their behavior may vary depending on the specific testing framework, project structure, and requirements. The provided code snippet serves as an example to showcase the concept of targeting specific channels during test execution.
