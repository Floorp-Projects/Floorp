# Physical Device Testing

We rely on [Firebase Test Lab][firebase] for automated on-device testing.

[firebase]: https://firebase.google.com/docs/test-lab/

Fenix UI tests use virtual devices by default.
These are run automatically on all Pull Requests and merges into the `main` branch.
Virtual devicesare quick to spin up, there is basically no upper limit of devices that can spawn on the cloud infrastructure and they usually produce the same result as running the test on a physical device.

However it is possible to run the tests on physical devices as well.
This requires some changes to the CI configuration.

> **NOTE**: Do not land a Pull Request that switches CI from virtual to physical devices!
> Add the `pr:do-not-land` label and call out that the PR is only there for testing!

By default the Fenix CI runs tests using virtual devices on `x86`.
That's faster when the host is also a `x86(_64)` system, but most physical devices use the Arm platform.
So first we need to instruct it to run tests on Arm.

Which platform to test on is defined in [`taskcluster/kinds/ui-test/kind.yml`](https://searchfox.org/mozilla-central/source/taskcluster/kinds/ui-test/kind.yml).
Find the line where it downloads the `target.apk` produced in a previous step and change it from `x86` to `arm64-v8a`:

```patch
  run:
      commands:
-         - [wget, {artifact-reference: '<signing/public/build/target.x86.apk>'}, '-O', app.apk]
+         - [wget, {artifact-reference: '<signing/public/build/target.arm64-v8a.apk>'}, '-O', app.apk]
```

Then look for the line where it invokes the `ui-test.sh` and tell it to use `arm64-v8a` again:

```patch
  run:
      commands:
-         - [automation/taskcluster/androidTest/ui-test.sh, x86, app.apk, android-test.apk, '-1']
+         - [automation/taskcluster/androidTest/ui-test.sh, arm64-v8a, app.apk, android-test.apk, '-1']
```

With the old CI configuration it will look for Firebase parameters in `automation/taskcluster/androidTest/flank-x86.yml`.
Now that we switched the architecture it will pick up [`automation/taskcluster/androidTest/flank-arm64-v8a.yml`](https://github.com/mozilla-mobile/fenix/blob/58e12b18e6e9f4f67c059fe9c9bf9f02579a55db/automation/taskcluster/androidTest/flank-arm64-v8a.yml) instead.

In that file we can now pick the device we want to run on:

```patch
   device:
-   - model: Pixel2
+   - model: dreamlte
      version: 28
```

You can get a [list of available devices](https://firebase.google.com/docs/test-lab/android/available-testing-devices) by running `gcloud` locally:

```
gcloud firebase test android models list
```

The value from the `MODEL_ID` column is what you use for the `model` parameter in `flank-arm64-v8a.yml`.
`dreamlte` translates to a Samsung Galaxy S8, which is available on Android API version 28.

If you only want to run a subset of tests define the `test-targets`:

```yaml
test-targets:
 - class org.mozilla.fenix.ui.BookmarksTest
```

Specify an exact test class as above to run tests from just that class.

And that's all the configuration necessary.
Save your changes, commit them, then push up your code and create a pull request.
Once the decision task on your PR finishes you will find a `ui-test-x86-debug` job (yes, `x86`, we didn't rename the job).
Its log file will have details on the test run and contain links to the test run summary.
Follow them to get more details, including the logcat output and a video of the test run.
