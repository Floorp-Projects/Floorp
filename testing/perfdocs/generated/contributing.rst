############
Contributing
############

Raptor on Mobile projects (Fenix, Reference-Browser)
****************************************************

Add new tests
-------------

For mobile projects, Raptor tests are on the following repositories:

**Fenix**:

- Repository: `Github <https://github.com/mozilla-mobile/fenix/>`__
- Tests results: `Treeherder view <https://treeherder.mozilla.org/#/jobs?repo=fenix>`__
- Schedule: Every 24 hours `Taskcluster force hook <https://tools.taskcluster.net/hooks/project-releng/cron-task-mozilla-mobile-fenix%2Fraptor>`_

**Reference-Browser**:

- Repository: `Github <https://github.com/mozilla-mobile/reference-browser/>`__
- Tests results: `Treeherder view <https://treeherder.mozilla.org/#/jobs?repo=reference-browser>`__
- Schedule: On each push

Tests are now defined in a similar fashion compared to what exists in mozilla-central. Task definitions are expressed in Yaml:

* https://github.com/mozilla-mobile/fenix/blob/1c9c5317eb33d92dde3293dfe6a857c279a7ab12/taskcluster/ci/raptor/kind.yml
* https://github.com/mozilla-mobile/reference-browser/blob/4560a83cb559d3d4d06383205a8bb76a44336704/taskcluster/ci/raptor/kind.yml

If you want to test your changes on a PR, before they land, you need to apply a patch like this one: https://github.com/mozilla-mobile/fenix/pull/5565/files. Don't forget to revert it before merging the patch. Note that the checks will run but the results aren't currently available on treeherder (`bug 1593252 <https://bugzilla.mozilla.org/show_bug.cgi?id=1593252>`_ is expected to address this).

On Fenix and Reference-Browser, the raptor revision is tied to the latest nightly of mozilla-central

For more information, please reach out to :mhentges in #cia

Code formatting on Raptor
*************************
As Raptor is a Mozilla project we follow the general Python coding style:

* https://firefox-source-docs.mozilla.org/tools/lint/coding-style/coding_style_python.html

`black <https://github.com/psf/black/>`_ is the tool used to reformat the Python code.
