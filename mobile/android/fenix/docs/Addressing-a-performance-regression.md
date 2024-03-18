To keep Fenix in a good shape, the performance team weekly runs multiple performance tests to identify regressions. The results are kept and maintained [here](https://earthangel-b40313e5.influxcloud.net/d/s3AT6t7nk/fenix-startup-nightly-via-backfill-py?orgId=1).

The main tests are:

* MAIN first frame:
Simulates a COLD MAIN (app icon launch) launch to report FullyDrawn, e.g when the user sees the app fully drawn after launching it.

* COLD VIEW Nav start:
An app link launch to load a page. It measures until "navigation starts" which is an internal Gecko event that indicates we're starting to load a page.

### What to do after a performance regression is reported.

Weekly after, the performance team runs the tests, if there is any regression, they will open a ticket, providing the dates between when was the last non-regressing and regressing version (an [example](https://github.com/mozilla-mobile/fenix/issues/25253) ticket). **These dates are important** for us to discover which commit introduced the regression. As we would like to identify which commit is the offending one, we need to bitset the commit range from non-regressing to regressing version. Fortunately for us, the performance team has a tool that can help us with that it’s called [backfil](https://github.com/mozilla-mobile/perf-tools/blob/main/backfill.py).

The tool can take a commit range start/end, build all the APKs, run the performance tests and provide the same data that it’s plotted [here](https://earthangel-b40313e5.influxcloud.net/d/s3AT6t7nk/fenix-startup-nightly-via-backfill-py?orgId=1). With it we can identify the offending commit.

### Install the required dependencies.

* Pull the [perf-tools](https://github.com/mozilla-mobile/perf-tools) repository, and on it follow the [configuration instructions](https://github.com/mozilla-mobile/perf-tools#configuration).
* Make you can run `adb` on the terminal, as the performance tools we use it through the scripts.

### Finding the regressing commit.

Now that you have all the dependencies installed, we will need to find the commit hash, where there the regression was reported and when the regression was not present, as [backfill](https://github.com/mozilla-mobile/perf-tools/blob/main/backfill.py), need them as parameters.

On the reported [ticket](https://github.com/mozilla-mobile/fenix/issues/25253), the performance team gave us the date when the regression was introduced (5/10), and when it wasn't present (5/9).

![image](https://user-images.githubusercontent.com/773158/174879320-ace21f51-2892-4b1c-8c25-a10ee8d0174e.png)

We can find each commit hash date by downloading the APKs, from Task Cluster and going to the about page.

For example:

**For 05/10**.
[https://firefox-ci-tc.services.mozilla.com/tasks/index/mobile.v2.fenix.nightly.2022.05.10.latest/armeabi-v7a](https://firefox-ci-tc.services.mozilla.com/tasks/index/mobile.v2.fenix.nightly.2022.06.06.latest/armeabi-v7a)

<img width="503" alt="image" src="https://user-images.githubusercontent.com/773158/174894842-3341aff6-185f-4402-9c30-3cd5c56dcd3d.png">

From it, we can find [2f7f5988f](https://github.com/mozilla-mobile/fenix/commit/2f7f5988fccad2cf2043eed4b6849b32a4c76048) when the regression was spotted.

**For 05/09**.
[https://firefox-ci-tc.services.mozilla.com/tasks/index/mobile.v2.fenix.nightly.2022.05.10.latest/armeabi-v7a](https://firefox-ci-tc.services.mozilla.com/tasks/index/mobile.v2.fenix.nightly.2022.06.06.latest/armeabi-v7a)

<img width="499" alt="image" src="https://user-images.githubusercontent.com/773158/174894896-a3a66219-ba0e-4174-b4fa-842e049e8d6d.png">

When the regression was not present [98455c01e](https://github.com/mozilla-mobile/fenix/commit/98455c01eeba7c63775f18817cd079f5d08b4513)

Using the commits we can construct this range:
https://github.com/mozilla-mobile/fenix/compare/98455c01eeba7c63775f18817cd079f5d08b4513...2f7f5988fccad2cf2043eed4b6849b32a4c76048
<img width="856" alt="commit range" src="https://user-images.githubusercontent.com/773158/175076734-9c585df2-7c5d-4b17-9135-9447643fb5d0.png">



With it we can see each commit that could introduced the regression.

### Using backfill.py

With the info that we found above, execute `backfill.py`

```
perf-tools-main % python3 backfill.py --tests cold_main_first_frame --startcommit 98455c01eeba7c63775f18817cd079f5d08b4513 --endcommit 2f7f5988fccad2cf2043eed4b6849b32a4c76048 --git_remote_name https://github.com/mozilla-mobile/fenix.git --repository_to_test_path ../fenix fenix nightly armeabi-v7a commitsRange
```

Where:
*  **cold_main_first_frame**: it's the test we would like to run, we could also pass `cold_view_nav_start` depending on the regression type.
*  **--startcommit**: it's the commit before the regression.
*  **--endcommit** it's the commit where the regression appears.
*  **--repository_to_test_path** is the path where your local Fenix repository is.


**Note**: Make sure your repository includes all the tokens (Sentry, Nimbus, … etc) that we include in our release builds, as not adding them could affect the test results, as we want the APKs to be the same [experience as normal users will have](https://wiki.mozilla.org/Performance/Fenix#How_to_measure_what_users_experience). Part of this is making sure you have [autosignReleaseWithDebugKey in your local.properties](https://github.com/mozilla-mobile/fenix#automatically-sign-release-builds).


🕐 Be patient, as we will have to build an APK for each possible commit in the range and for this range there [are 19 commits](https://github.com/mozilla-mobile/fenix/compare/98455c01eeba7c63775f18817cd079f5d08b4513...2f7f5988fccad2cf2043eed4b6849b32a4c76048) then we will build 19 APKs, and run the performance test for each one.


As the script progress, we will start to see some activity on the `perf-tools` **directory**, as each APKs will go there with the format  `apk_commit_<HASH>.apk`

![image](https://user-images.githubusercontent.com/773158/174896260-7273afae-02de-49bf-be78-da0e08aa05ff.png)

After all the APKs are built, the script will continue with the testing phase, it will run the tests per each commit/APKs, and create a directory named `backfill_output` where it will create two **.txt files per commit**  `apk_commit_<HASH>-cold_main_first_frame-analysis.txt` and `apk_commit_<HASH>-cold_main_first_frame-durations.txt`

![image](https://user-images.githubusercontent.com/773158/174896536-6b798764-b81d-4e3a-bab0-1a1a31a57664.png)

These files are the output of the script:

* **Cold_main_first_frame-analysis.txt**: Will contain key information about the test results like max,mean,median, and min.

* **Cold_main_first_frame-durations.txt**: Will contain the raw information of each repetition of the test.


With these files, we can identify which commit, introduced the regression by checking file by file which results are closer to the ones reported one the regression ticket.

After we found the regressing commit, we just have to update the ticket, posting our finding and tagging the person that introduced to research how to optimize the patch. Normally if the regression is significant we will ask to undo the commit until the patch is optimized.

## Extra tips:
* In case you would like to run the same test that are run via `backfil` for an specific APK, you can find more information [here](https://wiki.mozilla.org/Performance/Fenix/Performance_reviews#Measuring_cold_start_up_duration).
* If you would like to graph the results you can use `python3 analyze_durations.py --graph results.txt`.
*Just keep in mind, the results provide from the Performance team are from running the tests on a Moto G 5. Running on a more powerful device could cause the results to diverge.
* In case, you need to start recording a profile in startup https://profiler.firefox.com/docs/#/./guide-startup-shutdown?id=firefox-for-android.


### Identifying the source of the regression.
Our main task, when looking for a performance regression is just to identify the faulty commit, but if we would like to figure out what is the exact cause, we will need to take [profile](https://wiki.mozilla.org/Performance/Fenix/Performance_reviews#Profile) from regressing version, and the version before to try to identify what could be causing the issue, checking the code path of the regressing commit could give us some hints to where to look.
