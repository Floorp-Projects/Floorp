# Submitting patches

You can submit patches by using [Phabricator]. Walk through its documentation
in how to set it up, and uploading patches for review. Don't worry about which
person to select for reviewing your code. It will be done automatically.

Please also make sure to follow the [commit creation guidelines].

Once you have contributed a couple of patches, we are happy to
sponsor you in [becoming a Mozilla committer].  When you have been
granted commit access level 1 you will have permission to use the
[Firefox CI] to trigger your own “try runs” to test your changes.

You can use the `remote-protocol` [try preset]:

```shell
% ./mach try --preset remote-protocol
```

This preset will schedule tests related to the Remote Protocol component on
various platforms. You can reduce the number of tasks by filtering on platforms
(e.g. linux) or build type (e.g. opt):

```shell
% ./mach try --preset remote-protocol -xq "'linux 'opt"
```

But you can also schedule tests by selecting relevant jobs yourself:

```shell
% ./mach try fuzzy
```

[Phabricator]: https://moz-conduit.readthedocs.io/en/latest/phabricator-user.html
[commit creation guidelines]: https://mozilla-version-control-tools.readthedocs.io/en/latest/devguide/contributing.html#submitting-patches-for-review
[becoming a Mozilla committer]: https://www.mozilla.org/en-US/about/governance/policies/commit/
[Firefox CI]: https://treeherder.mozilla.org/
[try preset]: /tools/try/presets
