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

4. Once a Firefox Android team member has reviewed the PR and deemed it safe, comment the following to start UI tests.
```bors try```

5. Once the UI tests have all completed and passed, approve the PR and add `approved` and `needs landing` label the contributor's PR.

6. Monitor the merging process to make sure it lands as expected.

## Process for updating contributor PR (if contributor needs help or is unresponsive)

```sh
git remote add <Contributor remote name> <Contributor repository>
git checkout <Contributor remote name>/<Contributor branch name>
git rebase upstream/main (or any other actions you want to fixup in their PR)
git push <Contributor remote name> HEAD:<Contributor branch name> -f
```
