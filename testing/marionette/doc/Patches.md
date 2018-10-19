Submitting patches
==================

You can submit patches by using [Phabricator]. Walk through its documentation
in how to set it up, and uploading patches for review. Available reviewers are
_ato_, _automatedtester_, or _whimboo_.

Please also make sure to follow the [commit creation guidelines].

Once you have contributed a couple of patches, we are happy to
sponsor you in [becoming a Mozilla committer].  When you have been
granted commit access level 1 you will have permission to use the
[Firefox CI] to trigger your own “try runs” to test your changes.

This is a good try syntax to use when testing Marionette changes:

	-b do -p linux,linux64,macosx64,win64,android-api-16 -u marionette-e10s,marionette-headless-e10s,xpcshell,web-platform-tests,firefox-ui-functional -t none

[Phabricator]: https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html
[commit creation guidelines]: https://mozilla-version-control-tools.readthedocs.io/en/latest/devguide/contributing.html?highlight=phabricator#submitting-patches-for-review
[becoming a Mozilla committer]: https://www.mozilla.org/en-US/about/governance/policies/commit/
[Firefox CI]: https://treeherder.mozilla.org/
