Nested Doc Trees
================

This feature essentially means we can now group related docs together under
common "landing pages". This will allow us to refactor the docs into a structure that makes more sense. For example we could have a landing page for docs describing Gecko's internals, and another one for docs describing developer workflows in `mozilla-central`.


To clarify a few things:

#. The path specified in `SPHINX_TREES` does not need to correspond to a path in `mozilla-central`. For example, I could register my docs using `SPHINX_TREES["/foo"] = "docs"`, which would make that doc tree accessible at `firefox-source-docs.mozilla.org/foo`.

#. Any subtrees that are nested under another index will automatically be hidden from the main index. This means you should make sure to link to any subtrees from somewhere in the landing page. So given my earlier doc tree at `/foo`, if I now created a subtree and registered it using `SPHINX_TREES["/foo/bar"] = "docs"`, those docs would not show up in the main index.

#. The relation between subtrees and their parents does not necessarily have any bearing with their relation on the file system. For example, a doc tree that lives under `/devtools` can be nested under an index that lives under `/browser`.
