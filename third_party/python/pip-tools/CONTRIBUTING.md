[![Jazzband](https://jazzband.co/static/img/jazzband.svg)](https://jazzband.co/)

This is a [Jazzband](https://jazzband.co/) project. By contributing you agree
to abide by the [Contributor Code of Conduct](https://jazzband.co/about/conduct)
and follow the [guidelines](https://jazzband.co/about/guidelines).

## Project Contribution Guidelines

Here are a few additional or emphasized guidelines to follow when contributing to pip-tools:
- Install pip-tools in development mode and its test dependencies with `pip install -e .[testing]`.
- Check with `tox -e checkqa` to see your changes are not breaking the style conventions.
- Always provide tests for your changes.
- Give a clear one-line description in the PR (that the maintainers can add to [CHANGELOG](CHANGELOG.md) afterwards).
- Wait for the review of at least one other contributor before merging (even if you're a Jazzband member).
- Before merging, assign the PR to a milestone for a version to help with the release process.

The only exception to those guidelines is for trivial changes, such as
documentation corrections or contributions that do not change pip-tools itself.

Contributions following these guidelines are always welcomed, encouraged and appreciated.

## Project Release Process

Jazzband aims to give full access to all members, including performing releases, as described in the
[Jazzband Releases documentation](https://jazzband.co/about/releases).

To help keeping track of the releases and their changes, here's the current release process:
- Check to see if any recently merged PRs are missing from the milestone of the version about to be released.
- Create a branch for the release. *Ex: release-3.4.0*.
- Update the [CHANGELOG](CHANGELOG.md) with the version, date and using the one-line descriptions
  from the PRs included in the milestone of the version.
  Check the previous release changelog format for an example. Don't forget the "Thanks @contributor" mentions.
- Push the branch to your fork and create a pull request.
- Merge the pull request after the changes being approved.
- Make sure that the tests/CI still pass.
- Once ready, go to `Github pip-tools Homepage > releases tab > Draft a new release` and type in:
  - *Tag version:* The exact version number, following [Semantic Versioning](https://blog.versioneye.com/2014/01/16/semantic-versioning/). *Ex: 3.4.0*
  - *Target:* master. As a general rule, the HEAD commit of the master branch should be the release target.
  - *Release title:* Same as the tag. *Ex: 3.4.0*
  - *Describe this release:* Copy of this release's changelog segment.
- Publish release. This will push a tag on the HEAD of master, trigger the CI pipeline and
  deploy a pip-tools release in the **Jazzband private package index** upon success.
- The pip-tools "lead" project members will receive an email notification to review the release and
  deploy it to the public PyPI if all is correct.
- Once the release to the public PyPI is confirmed, close the milestone.

Please be mindful of other before and when performing a release, and use this access responsibly.

Do not hesitate to ask questions if you have any before performing a release.
