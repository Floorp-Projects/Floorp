# Contributing code to Mozilla's Android projects
Thank you for taking the time to contribute to one of Mozilla's Android
projects! 🔥 🦊 ❤️ 🤖! 🎉 👍 For a full list of projects, see
[the README](../../../README.md).

Before contributing, please review our [Community Participation Guidelines].

If you run into trouble at any point, ask for help! Check out our
[preferred communication channels.](./CONTRIBUTING.md#communication)

Contents:
- [Beginner's guides](#beginners-guides)
- [Configuring Android Studio](#configuring-android-studio)
- [Finding issues to work on](#finding-issues-to-work-on)
- [Creating a Pull Request](#creating-a-pull-request)
- [Merging](#merging)
- [Writing tests](#writing-tests)

## Beginner's guides
Unfamiliar with the technology we use? No problem! We were once new to this
too! Here are few guides we've compiled to help you get started:
- [Git guide](../git_guide.md#android)
- [Android guide](android_guide.md)
  - [Accessibility guide](accessibility_guide.md)
- [Kotlin guide](kotlin_guide.md)
- [Writing custom lint rules guide](writing_lint_rules.md)
- [Taskcluster guide](taskcluster_guide.md)
- [MockK guide](https://notwoods.github.io/mockk-guidebook/)

If these are confusing or if you have questions, please let us know!

## Configuring Android Studio
We don't allow wildcard imports (ex: `import kotlinx.coroutines.*`) so we recommend preventing Android Studio from auto-importing or optimizing with them. This can be done via the `Preferences > Editor > Kotlin > Code Style > Use single name import` option.

## Finding issues to work on
**New to Mozilla's mobile projects?** See issues labeled `good first issue` in your project's
issues tracker (example: [focus-android][fa good first]). These are designed to be
easier to implement so you can focus on learning our pull request workflow. *Please only
fix one of these.*

**Looking for more challenging issues?** See issues labeled `help wanted` (example:
[focus-android][fa help]). These are issues that are ready to be implemented without
additional product or UX discussion.

**When you find an issue you'd like to work on,** *comment on the issue* saying that
you'd like to work on it. This ensures it is still available for you to work on.

**If you want to work on a new feature**, *always file an issue first* and wait
for our team to discuss it. We want to ensure all teams (product, ux, engineering)
have an opportunity to provide feedback. **Pull requests for unsolicited features
are unlikely to get merged.**

## Creating a Pull Request
Our team follows [the GitHub pull request workflow][gh workflow]: fork, branch, commit,
pull request, automated tests, review, merge. If you're new to GitHub, check out [the official
guides][gh guides] for more information.

An example commit message summary looks like, `For #5: Upgrade gradle to v1.3.0`.

Please follow these guidelines for your pull requests:

- All Pull Requests should address an issue. If your pull request doesn't have an
issue, file it!
  - GitHub search defaults to issues, not PRs, so ensuring there is an issue for your PR
  means it'll be easier to find
- The commit message summary should briefly describe what code changed in the commit, *not
the issue you're fixing.*
  - We encourage you to use the commit message body to elaborate what changed and why
- Include the issue number in your commit messages. This links your PR to the issue it's
intended to fix.
  - If your PR closes an issue, include `Closes #...` in one of your commit messages. This
  will automatically close the linked issue ([more info][auto close]).
  - If your PR has to go through a longer process, for example QA verification, use the
  `For #...` syntax to allow the linked issue to be closed at a later, more appropriate time.
- Prefer "micro commits".
  - A micro commit is a small commit that generally changes one thing.
  A single Pull Request may comprise of multiple incremental micro commits.
  - A series of micro commits should tell a story. For example, if your goal is to add a new
  icon to the toolbar, you can make a commit to add the icon asset and then make a commit to
  use the icon in the code.
  - Commits should generally not undo the work of previous commits in the same PR.
  - If you're not comfortable making micro commits, it's okay to begin contributing without
  them.
- Add a reviewer to ensure someone sees, and reviews, your pull request so it can be merged
- If the tests fail, please try to fix them! Keeping the tests passing ensures our code isn't
broken and the code is unlikely to get merged without passing tests. If you run into trouble,
ask for help!
- If there are UI changes, include a screenshot so UX can also do a visual review
- When in doubt, look at the closed PRs in the repository to follow as an example or ask
us online!

If your code is not approved, address the suggested comments, push your changes, and re-request
review from your reviewer again.

## Merging
After your code has been approved and the tests pass, your code will be merged into master
by the core team. When merging, we use GitHub's "Rebase and merge":
- We keep a linear git history for readability
- We prefer incremental commits to remain in the history
  - It's easier to read, helps with bisection, and matches repo state during review.

## Writing tests
To learn more about how our tests are structured, see [testing.md](testing.md).

## Stale PRs
If changes are requested in your patch and there is no indication of further activity within 2 weeks,
we may close your PR in order to keep our repo in a manageable state. This does not mean we are not
interested in your patch! Once the requested changes are made, feel free to re-open the PR.

Conversely, if your PR is not receiving attention from the organization, here are a few strategies
for escalating it:
- Make sure your PR has the `needs-review` label.
- Tag specific reviewers. Look for people that recently touched the file, that are active in other patches, or the component triage owner for the associated bug in Bugzilla.
- Reach out to us on [Matrix](https://matrix.to/#/#fenix:mozilla.org).

[Community Participation Guidelines]: https://www.mozilla.org/en-US/about/governance/policies/participation/
[fa good first]: https://github.com/mozilla-mobile/focus-android/labels/good%20first%20issue
[fa help]: https://github.com/mozilla-mobile/focus-android/labels/help%20wanted
[gh workflow]: https://guides.github.com/introduction/flow/
[gh guides]: https://guides.github.com/
[auto close]: https://help.github.com/articles/closing-issues-using-keywords/
