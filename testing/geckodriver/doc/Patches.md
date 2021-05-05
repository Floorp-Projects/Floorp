Submitting patches
==================

You can submit patches by using [Phabricator]. Walk through its documentation
in how to set it up, and uploading patches for review. Don't worry about which
person to select for reviewing your code. It will be done automatically.

Please also make sure to follow the [commit creation guidelines].

Once you have contributed a couple of patches, we are happy to sponsor you in
[becoming a Mozilla committer].  When you have been granted commit access
level 1, you will have permission to use the [Firefox CI] to trigger your own
“try runs” to test your changes. You can use the following [try preset] to run
the most relevant tests:

	mach try --preset geckodriver

This preset will schedule geckodriver-related tests on various platforms. You can
reduce the number of tasks by filtering on platforms (e.g. linux) or build type
(e.g. opt):

	mach try --preset geckodriver -xq "'linux 'opt"

[Phabricator]: https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html
[commit creation guidelines]: https://mozilla-version-control-tools.readthedocs.io/en/latest/devguide/contributing.html?highlight=phabricator#submitting-patches-for-review
[becoming a Mozilla committer]: https://www.mozilla.org/en-US/about/governance/policies/commit/
[Firefox CI]: https://treeherder.mozilla.org/
[try preset]: https://firefox-source-docs.mozilla.org/tools/try/presets.html
