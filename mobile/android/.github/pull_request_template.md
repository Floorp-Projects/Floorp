

### Pull Request checklist
<!-- Before submitting the PR, please address each item -->
- [ ] **Quality**: This PR builds and passes detekt/ktlint checks (A pre-push hook is recommended)
- [ ] **Tests**: This PR includes thorough tests or an explanation of why it does not
- [ ] **Changelog**: This PR includes [a changelog entry](https://github.com/mozilla-mobile/firefox-android/blob/main/docs/changelog.md) or does not need one
- [ ] **Accessibility**: The code in this PR follows [accessibility best practices](https://github.com/mozilla-mobile/firefox-android/blob/main/docs/shared/android/accessibility_guide.md) or does not include any user facing features

### After merge
- **Breaking Changes**: If this is a breaking Android Components change, please push a draft PR on [Reference Browser](https://github.com/mozilla-mobile/reference-browser) to address the breaking issues.

### To download an APK when reviewing a PR (after all CI tasks finished running):
1. Click on `Checks` at the top of the PR page.
2. Click on the `firefoxci-taskcluster` group on the left to expand all tasks.
3. Click on the `build-apk-{fenix,focus,klar}-debug` task you're interested in.
4. Click on `View task in Taskcluster` in the new `DETAILS` section.
5. The APK links should be on the right side of the screen, named for each CPU architecture.

### GitHub Automation
<!-- Do not add anything below this line -->

Used by GitHub Actions.
