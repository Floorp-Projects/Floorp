Submitting patches
==================

You can submit patches by uploading .diff files to Bugzilla or by
sending them to [MozReview].

Once you have contributed a couple of patches, we are happy to
sponsor you in [becoming a Mozilla committer].  When you have been
granted commit access level 1 you will have permission to use the
[Firefox CI] to trigger your own “try runs” to test your changes.

This is a good try syntax to use when testing Marionette changes:

    -b do -p linux,linux64,macosx64,win64,android-api-16 -u marionette-e10s,marionette-headless-e10s,xpcshell,web-platform-tests,firefox-ui-functional -t none

[MozReview]: http://mozilla-version-control-tools.readthedocs.io/en/latest/mozreview.html
[becoming a Mozilla committer]: https://www.mozilla.org/en-US/about/governance/policies/commit/
[Firefox CI]: https://treeherder.mozilla.org/
