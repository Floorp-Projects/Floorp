# Guide to merging contributor PRs for Firefox Android team members

Contributor PRs will run only a specific suite of CI tests (excluding UI tests) in order to protect secrets. Use the following steps when reviewing and merging a contributor PR.

## Process for landing contributor PR
_Note: these instructions use https://cli.github.com/_

1. Fetch upstream changes and locally check out the contributor's branch onto your fork.
```sh
git fetch --all
gh pr checkout <PR number>

# Example:
gh pr checkout 1234 # for https://github.com/mozilla-mobile/firefox-android/pull/1234
```
2. Build and run contributor's changes locally to verify that it works correctly.

3. Review the code to make sure everything is clean.

4. ~~Once a Firefox Android team member has reviewed the PR and deemed it safe, comment the following to start UI tests.
   ```bors try```~~  
   `Bors` public instance is now offline, as they announced in [their newsletter](https://bugzilla.mozilla.org/show_bug.cgi?id=1850420). Please refer to section "Process for running UI tests on a contributor PR" above.


5. Once the UI tests have all completed and passed, approve the PR and add `approved` and `needs landing` label the contributor's PR.

6. Monitor the merging process to make sure it lands as expected.

## Process for running UI tests on a contributor PR

1. Make sure you have already checked out the contributor's branch onto your fork, following the previous session's first step.
```sh
git fetch --all
gh pr checkout <PR number>

# Example:
gh pr checkout 1234 # for https://github.com/mozilla-mobile/firefox-android/pull/1234
```

3. Rename your local branch's to any name
```
git branch -m <new branch name>

# Example: If the contributor's branch is named `eliserichards:my-fun-branch1`
git branch -m ci-for-my-fun-branch1
```

3. Push branch to your fork using any branch name:
```sh
git push origin <pr-branch-name>

# Example: If the contributor's branch is named `eliserichards:my-fun-branch1`
git push origin ci-for-my-fun-branch1
```

4. Create a PR from _your fork's_ copy of the branch e.g. https://github.com/mozilla-mobile/firefox-android/compare/main...eliserichards:my-fun-branch1

* Please note in the PR description which PR you are running CI for. Example: https://github.com/mozilla-mobile/firefox-android/pull/4577

***

**Once you create this PR, the CI for both the original and the duplicate PRs will run. When everything is green, you can merge either of them.**

5. To land the original:
* i. Make sure that contributor's branch hasn't diverged from yours (they must have the same SHA).
* ii. The change has to be on the top of the main branch when it is first in line in the merge queue.
* iii. It requires the needs-landing label.

**NB**: Adding `needs-landing` label while failing to ensure the same SHA will block the mergify queue and will require manual intervention: mergify will trigger CI for the original PR again and wait for it to finish, but CI won’t run all the checks because there is no PR with the same SHA any more that backs it up. If that happens, talk to the release team.


## Process for updating contributor PR (if contributor needs help or is unresponsive)

```sh
git remote add <Contributor remote name> <Contributor repository>
git checkout <Contributor remote name>/<Contributor branch name>
git rebase upstream/main (or any other actions you want to fixup in their PR)
git push <Contributor remote name> HEAD:<Contributor branch name> -f
```
