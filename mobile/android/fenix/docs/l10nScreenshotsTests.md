### Screenshtos Tests
We are using [`screengrab`](https://docs.fastlane.tools/getting-started/android/screenshots/) which works with fastlane to automate the process of capturing screenshots.
All the l10n screenshots are generated through the ui tests. These particular tests run as part of the screenshots package (`app/src/androidTest/mozilla/fenix/ui/screenshots`)

### Run tests locally from Android Studio
Navigate to `app/src/androidTest/mozilla/fenix/ui/screenshots`, once in that directory, run the full test suite or a specific test by clicking on the `>` button.

By running them manually you can check whether the test works or not but screenshots will not be saved.

### Run tests locally from command line
1. Install the gem:
`sudo gem install screengrab`

2. From command line run:
`fastlane screengrab --test_instrumentation_runner "androidx.test.runner.AndroidJUnitRunner"`

The package configuration, apk paths as well as the locales are set in [Screengrab file](https://github.com/mozilla-mobile/fenix/blob/073fd8939067bc7a367d8db497bcf53fbd24cdd2/fastlane/Screengrabfile#L5).
In case there is a change there the file has to be modified accordingly.
Before launching that command, there has to be an emulator running.

Once the build and tests finsish, screenshots will be saved in the root directory: `fastlane/metadata/android`
If there is a failure and screenshots are not saved, it may be necessary to create these folders manually first.

## Run tests on CI
Currently there is a cron job scheduled weekly to run these tests on Monday.
It is set [here](https://github.com/mozilla-mobile/fenix/blob/5a8a7f549946fc8ad6ccf31f8c9c6bc2180aaed2/.cron.yml#L27). And the test results can be seen in [treeherder](https://treeherder.mozilla.org/jobs?repo=fenix&fromchange=67fb033f1bc8b772b64a1fda410632dd7e59450e&selectedTaskRun=NngGd-kXTtGGDpI9sMl2Bw.0), see previous link as an example.

### TBD
So far, the tests run and the results are checked to be sure these tests work but the screenshots are not taken/saved on CI. That could be done in the future if there is a request from l10n team or any team interested in that.
See how it is done on [iOS](https://github.com/mozilla-mobile/firefox-ios/wiki/Screenshots-for-Localization) which integrates Taskcluster as job/task orchestrator, Bitrise to run the tests to take the screenshots and Taskcluster again to store and publicly share the screenshots.

For any doubt or comment about these tests, please contact [Mobile Test Engineering](https://mana.mozilla.org/wiki/display/MTE/Mobile+Test+Engineering) team, slack: [#mobile-testeng](https://mozilla.slack.com/archives/C02KDDS9QM9)
