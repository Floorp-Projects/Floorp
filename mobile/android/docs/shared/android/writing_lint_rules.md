# Writing Custom Lint Rules Guide
This guide will help you add custom lint rules to your application so you can catch errors at compile time rather than at run time or during testing.

If you get into trouble, _ask for help!_ These APIs can be confusing to get started with and it will save the team time if we share knowledge!

### Comparison of systems
We have a few common lint systems for Mozilla Android apps:
- **detekt:** (probably) the go-to for analyzing Kotlin code
- **ktlint:** similar to detekt but use it instead if you don't want your lint rule to be suppressed, ever
- **android lint:** most useful when analyzing more than Kotlin code: Android-specific concerns (e.g. resource XML files), Gradle files, ProGuard files, etc. It is slow because it needs to compile the project and does multiple passes over the code; however, if your new lint check needs multiple passes, it can be useful.
- **Gradle tasks + hand-rolled check:** less performant but probably more familiar for devs to write. Use if you can't figure out how to use one of the other tools or if you're short on time

## detekt
Be sure to read the gotchas section too.

To add a lint check:
- Add a class that extends `Rule`. You need to override an appropriate `visit*` function to analyze the code and call `report` from it to trigger a violation. `visit*` uses the Visitor pattern so this should be intuitive if you're familiar with it.
- Initialize this class in `CustomRulesetProvider.kt`
- Add your lint rule to `config/detekt.yml` with `active: true`

Here's [an example commit](https://github.com/mozilla-mobile/fenix/pull/15570/commits/5c438c918e640b2319b183ca1207bdc1667f62b1) where a lint rule is added and [the official docs](https://detekt.github.io/detekt/extensions.html).

### Gotchas
Modifying lint rules will cause `./gradlew detekt` to fail because it breaks the gradle daemon. To work around this, do one of:
1. During development of custom lint rules, run without the daemon `./gradlew --no-daemon detekt`
1. Restart the gradle daemon between runs: `./gradlew --stop`

The error you'll see if you reuse this invalid gradle daemon is:
```
Property 'mozilla-detekt-rules' is misspelled or does not exist.

FAILURE: Build failed with an exception.

* What went wrong:
Execution failed for task ':detekt'.
> Run failed with 1 invalid config property.
```

detekt is lacking good documentation for writing custom rules: to understand how to write lint rules, look at our custom rules and the source of the rules built into detekt. You can also use `println` to output values inside `visit*` methods to understand what is available (ideally we can figure out how to launch a debugger to analyze it at runtime but we haven't yet).

## ktlint
TODO: fill this out!

We haven't written any ktlint rules so we're lacking knowledge here.

## Android Lint
Custom Android lint checks are a great way to prohibit or warn about usages of certain classes and resources in a codebase and can be customized to give alternative suggestions.

To add a lint check:
- Create a custom Detector that will scour the codebase for Issues to report (and add tests!)
- Make sure to be specific for which resource files we want to even look at using `appliesTo` and which elements in those files we care about using `getApplicableElements`
- Add the new detector to the LintIssueRegistry.kt file

Here's [an example PR](https://github.com/mozilla-mobile/android-components/pull/6112) where lint rules are added (and tested!) to check XML files for incorrect usage.

Here's a good [conference talk](https://www.droidcon.com/media-detail?video=380845392) about adding your own custom lint rules and an accompanying [repo](https://github.com/alexjlockwood/android-lint-checks-demo) with some great examples of what lint checks can do.

## Gradle tasks + hand-rolled check
You can create Gradle tasks that, for example:
1. Read kotlin files into memory and analyze them line-by-line
1. Call out to grep, find, or other shell commands and analyze the output
1. Call out to custom python scripts

If a violation is found, you can fail the Gradle task, failing the build.

Unlike writing lint rules using other tools, there are some downsides:
- It's less performant: lint tools traverse the file hierarchy once so each lint rule we right adds an additional traverse, depending on the files analyzed
- It requires custom mechanisms to allow suppression of violations unlike in detekt, where you can simply declare `@Suppress("VIOLATION_NAME")`
- You have to know enough Gradle to get started. This is doable for straightforward cases but can quickly grow in complexity.
- You have to manually hook it into the correct build processes and this is tricky to do. Should it run in the pre-push hook? In TaskCluster CI (probably)? On every build?

For an example of a Gradle task doing static analysis, see [LintUnitTestRunner](https://github.com/mozilla-mobile/fenix/blob/4a06e40e70c37df4157efc7f90afb682dbd3bb54/buildSrc/src/main/java/org/mozilla/fenix/gradle/tasks/LintUnitTestRunner.kt#L29).
