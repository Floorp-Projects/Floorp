# Guide to merging contributor PRs for Firefox Android team members

Contributor PRs will run only a specific suite of CI tests (excluding UI tests) in order to protect secrets. Once a Firefox Android team member has reviewed the PR and deemed it safe, we can use the following steps to run the full CI suite and land the patch.

## Command line instructions

_Note: these instructions use https://cli.github.com/_

1. Fetch upstream changes and locally check out the contributor's branch onto your fork
```sh
git fetch --all
gh pr checkout <PR number>` or `gh pr checkout <PR number> -R mozilla-mobile/fenix

# Example:
gh pr checkout 12345 -R mozilla-mobile/fenix # for https://github.com/mozilla-mobile/fenix/pull/12345
```
2. Rename your local branch's name
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

4. Create a PR from _your fork's_ copy of the branch e.g. https://github.com/mozilla-mobile/fenix/compare/main...eliserichards:my-fun-branch1

* Please note in the PR description which PR you are running CI for. Example: https://github.com/mozilla-mobile/fenix/pull/27530

***

**Once you create this PR, the CI for both the original and the duplicate PRs will run. When everything is green, you can merge either of them.**

5. To land the duplicate, close the original PR first, refresh mergify (`@Mergifyio refresh`), and then add `needs-landing` label to your PR.

* Mergify won’t merge the duplicate while the original is open, since they both have the same SHA and mergify does honour the first one over those created consequently.

OR

5. To land the original:
* i. Make sure that contributor's branch hasn't diverged from yours (they must have the same SHA).
* ii. The change has to be on the top of the main branch when it is first in line in the merge queue.
* iii. It requires the needs-landing label.

**NB**: Adding `needs-landing` label while failing to ensure the same SHA will block the mergify queue and will require manual intervention: mergify will trigger CI for the original PR again and wait for it to finish, but CI won’t run all the checks because there is no PR with the same SHA any more that backs it up. If that happens, talk to the release team.
