These are instructions for preparing a release branch for Fenix Beta release. For reference, the AC release checklist can be found at https://mozac.org/contributing/release-checklist.

## [Release Management] Creating a new Beta release branch
**This part is 100% covered by the Release Management team. The dev team should not perform these steps.**

1. Create a branch name with the format `releases_v[beta_version].0.0` (for example: `releases_v87.0.0`) off of the `main` branch using the GitHub UI. `[beta_version]` should follow the Firefox Beta version number. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar).
2. Verify that `version.txt` already refers to the Fenix Beta release version `[beta_version].0b1` in the `releases_v[beta_version].0.0` branch. Bump it manually if necessary.
3. In the `main` branch, create a pull request to update `version.txt` to `[nightly_version].0b1`. Land the updated `version.txt` into `main` with a review from someone from RelMan (#releaseduty-mobile). See [#26177](https://github.com/mozilla-mobile/fenix/pull/26177) for example.
4. Notify the dev team that they can start the new Nightly development cycle per the steps given in the next section ⬇️
5. Once the new AC `v[beta_version].0b1` release is ready (see the [AC release checklist](https://mozac.org/contributing/release-checklist)), create a pull request targeting the `releases_v[beta_version].0.0` branch to pin the Android Components `VERSION` to `[beta_version].0b1`. The commit message should be `Set Android-Components version to [beta_version].0b1`.
```diff
diff --git a/buildSrc/src/main/java/AndroidComponents.kt b/buildSrc/src/main/java/AndroidComponents.kt
--- a/buildSrc/src/main/java/AndroidComponents.kt
+++ b/buildSrc/src/main/java/AndroidComponents.kt
@@ -3,5 +3,5 @@
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 object AndroidComponents {
-    const val VERSION = "110.0.20230115143320"
+    const val VERSION = "110.0b1"
 }
```
6. Announce the new `releases_v[beta_version].0.0` branch on Slack in #releaseduty-mobile.
7. Once the AC version bump has landed, create the new Fenix Beta release in [Ship-It](https://shipit.mozilla-releng.net/) per normal practice.

## [Dev Team] Starting the next Nightly development cycle
**Release Management will take care of updating `version.txt` for the `main` branch in order to prepare beta versions of our Nightly release.**

### [Dev Team] Create new milestone

Create a new [milestone](https://github.com/mozilla-mobile/fenix/milestones) for the `[nightly_version]` and close the existing `[beta_version]` milestone.

The milestone is an indicator of the Fenix version where the code related to the issue is landed, and does not need to reflect when the issue is closed by QA verified. This is useful for keeping track of when a feature is shipped.

Examine all the remaining open issues in the closed milestone to see if the issue should be closed or remove the tagged milestone depending on what is appropriate. If an issue is still in "eng:qa-needed", then it is fine to let it remain in the current closed milestone and open. If an issue clearly doesn't require "eng:qa-needed" (eg, Remove strings in 104, Fix typo, etc), then remove the label and close the issue. If an issue is clearly unresolved due to being reopened by QA and work still continues, remove the milestone.

### [Dev Team] Renew telemetry

After the Beta cut, another task is to renew/remove all soon to expire telemetry probes. What we're looking for is to create a list of telemetry that will expire in `[nightly_version add 2]`.  See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar) for the current Release version.  There is a script that will help with finding these soon to expire telemetry.

1. Use the helper in tools folder `python3 data_renewal_generate.py [nightly_version add 2]` to detected and generate files that will help create the following files:
 - `[nightly_version add 2]`_expiry_list.csv
 - `[nightly_version add 2]`_renewal_request.txt
2. Upload the `[nightly_version add 2]`_expiry_list.csv to Google sheet in this [shared Google Drive](https://drive.google.com/drive/folders/1_ertMvn59eE9JmN721RqOjW6nNtxq9oS?usp=sharing) and contact product to review.  For each telemetry listed answer decide for:
 - Renew the metric (Recommendation is to use nightly_version + 12)
 - Choose not to renew (but not delete)
 - Choose to remove the metric
 - Renew the metric and set to never expire (this should only be for business critical metrics)
3. Note that `metrics.yaml` is also modified.  Once the review is over, continue to modify `metrics.yaml` to match the decision made in the Google sheet.  Make sure to add the PR link and if the telemetry never expires, add the email of the owner as contact.
4. File an issue for telemetry renewal so that a patch can target it and assign the issue to Product for increased visibility, as a reminder to to address the expiring metrics. See [issue 28190](https://github.com/mozilla-mobile/fenix/issues/28190) for an example.
5. Create a PR for review.  Modify `[nightly_version add 2]`_renewal_request.txt and paste it to the PR for data review. This comment can be auto-generated using the filled `[nightly_version add 2]`_expiry_list.csv and the `tools/data_renewal_request.py` helper. Copy the filled CSV into the tools directory and run the script to create a `[nightly_version add 2]`_filled_renewal_request.txt file that will contain the text required for data review. Make sure it includes (or add manually if necessary):
 - When will this collection now expire?
 - Why was the initial period of collection insufficient?
6. Please also check if you're responsible for Focus telemetry renewal.

### [Dev Team] Remove unused strings

Now that we made the Beta cut, we can remove all the unused strings marked moz:removedIn <= `[release_version subtract 1]`. `[release_version]` should follow the Firefox Release version. See [Firefox Release Calendar](https://wiki.mozilla.org/Release_Management/Calendar) for the current Release version.

1. File a GitHub issue named "Remove all unused strings marked moz:removedIn <= `[release_version subtract 1]`".
2. Search and remove all strings marked `moz:removedIn="[release_version subtract 1]"`.
3. Put up a pull request.
4. Please also check if you're responsible for Focus as well.

### Ask for Help

If you run into any problems, please ask any questions on Slack in #releaseduty-mobile.
