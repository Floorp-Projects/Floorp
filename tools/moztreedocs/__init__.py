# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from pathlib import PurePath

import sphinx
import sphinx.ext.apidoc
import yaml
from mozbuild.base import MozbuildObject
from mozbuild.frontend.reader import BuildReader
from mozbuild.util import memoize
from mozpack.copier import FileCopier
from mozpack.files import FileFinder
from mozpack.manifests import InstallManifest

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

MAIN_DOC_PATH = os.path.normpath(os.path.join(build.topsrcdir, "docs"))

logger = sphinx.util.logging.getLogger(__name__)


@memoize
def read_build_config(docdir):
    """Read the active build config and return the relevant doc paths.

    The return value is cached so re-generating with the same docdir won't
    invoke the build system a second time."""
    trees = {}
    python_package_dirs = set()

    is_main = docdir == MAIN_DOC_PATH
    relevant_mozbuild_path = None if is_main else docdir

    # Reading the Sphinx variables doesn't require a full build context.
    # Only define the parts we need.
    class fakeconfig(object):
        topsrcdir = build.topsrcdir

    variables = ("SPHINX_TREES", "SPHINX_PYTHON_PACKAGE_DIRS")
    reader = BuildReader(fakeconfig())
    result = reader.find_variables_from_ast(variables, path=relevant_mozbuild_path)
    for path, name, key, value in result:
        reldir = os.path.dirname(path)

        if name == "SPHINX_TREES":
            # If we're building a subtree, only process that specific subtree.
            # topsrcdir always uses POSIX-style path, normalize it for proper comparison.
            absdir = os.path.normpath(os.path.join(build.topsrcdir, reldir, value))
            if not is_main and absdir not in (docdir, MAIN_DOC_PATH):
                # allow subpaths of absdir (i.e. docdir = <absdir>/sub/path/)
                if docdir.startswith(absdir):
                    key = os.path.join(key, docdir.split(f"{key}/")[-1])
                else:
                    continue

            assert key
            if key.startswith("/"):
                key = key[1:]
            else:
                key = os.path.normpath(os.path.join(reldir, key))

            if key in trees:
                raise Exception(
                    "%s has already been registered as a destination." % key
                )
            trees[key] = os.path.join(reldir, value)

        if name == "SPHINX_PYTHON_PACKAGE_DIRS":
            python_package_dirs.add(os.path.join(reldir, value))

    return trees, python_package_dirs


class _SphinxManager(object):
    """Manages the generation of Sphinx documentation for the tree."""

    NO_AUTODOC = False

    def __init__(self, topsrcdir, main_path):
        self.topsrcdir = topsrcdir
        self.conf_py_path = os.path.join(main_path, "conf.py")
        self.index_path = os.path.join(main_path, "index.rst")

        # Instance variables that get set in self.generate_docs()
        self.staging_dir = None
        self.trees = None
        self.python_package_dirs = None

    def generate_docs(self, app):
        """Generate/stage documentation."""
        if self.NO_AUTODOC:
            logger.info("Python/JS API documentation generation will be skipped")
            app.config["extensions"].remove("sphinx.ext.autodoc")
            app.config["extensions"].remove("sphinx_js")
        self.staging_dir = os.path.join(app.outdir, "_staging")

        logger.info("Reading Sphinx metadata from build configuration")
        self.trees, self.python_package_dirs = read_build_config(app.srcdir)

        logger.info("Staging static documentation")
        self._synchronize_docs(app)

        if not self.NO_AUTODOC:
            self._generate_python_api_docs()

    def _generate_python_api_docs(self):
        """Generate Python API doc files."""
        out_dir = os.path.join(self.staging_dir, "python")
        base_args = ["--no-toc", "-o", out_dir]

        for p in sorted(self.python_package_dirs):
            full = os.path.join(self.topsrcdir, p)

            finder = FileFinder(full)
            dirs = {os.path.dirname(f[0]) for f in finder.find("**")}

            test_dirs = {"test", "tests"}
            excludes = {d for d in dirs if set(PurePath(d).parts) & test_dirs}

            args = list(base_args)
            args.append(full)
            args.extend(excludes)

            sphinx.ext.apidoc.main(argv=args)

    def _synchronize_docs(self, app):
        m = InstallManifest()

        with open(os.path.join(MAIN_DOC_PATH, "config.yml"), "r") as fh:
            tree_config = yaml.safe_load(fh)["categories"]

        m.add_link(self.conf_py_path, "conf.py")

        for dest, source in sorted(self.trees.items()):
            source_dir = os.path.join(self.topsrcdir, source)
            for root, _, files in os.walk(source_dir):
                for f in files:
                    source_path = os.path.normpath(os.path.join(root, f))
                    rel_source = source_path[len(source_dir) + 1 :]
                    target = os.path.normpath(os.path.join(dest, rel_source))
                    m.add_link(source_path, target)

        copier = FileCopier()
        m.populate_registry(copier)

        # In the case of livereload, we don't want to delete unmodified (unaccounted) files.
        copier.copy(
            self.staging_dir, remove_empty_directories=False, remove_unaccounted=False
        )

        with open(self.index_path, "r") as fh:
            data = fh.read()

        def is_toplevel(key):
            """Whether the tree is nested under the toplevel index, or is
            nested under another tree's index.
            """
            for k in self.trees:
                if k == key:
                    continue
                if key.startswith(k):
                    return False
            return True

        def format_paths(paths):
            source_doc = ["%s/index" % p for p in paths]
            return "\n   ".join(source_doc)

        toplevel_trees = {k: v for k, v in self.trees.items() if is_toplevel(k)}

        CATEGORIES = {}
        # generate the datastructure to deal with the tree
        for t in tree_config:
            CATEGORIES[t] = format_paths(tree_config[t])

        # During livereload, we don't correctly rebuild the full document
        # tree (Bug 1557020). The page is no longer referenced within the index
        # tree, thus we shall check categorisation only if complete tree is being rebuilt.
        if app.srcdir == self.topsrcdir:
            indexes = set(
                [
                    os.path.normpath(os.path.join(p, "index"))
                    for p in toplevel_trees.keys()
                ]
            )
            # Format categories like indexes
            cats = "\n".join(CATEGORIES.values()).split("\n")
            # Remove heading spaces
            cats = [os.path.normpath(x.strip()) for x in cats]
            indexes = tuple(set(indexes) - set(cats))
            if indexes:
                # In case a new doc isn't categorized
                print(indexes)
                raise Exception(
                    "Uncategorized documentation. Please add it in docs/config.yml"
                )

        data = data.format(**CATEGORIES)

        with open(os.path.join(self.staging_dir, "index.rst"), "w") as fh:
            fh.write(data)


manager = _SphinxManager(build.topsrcdir, MAIN_DOC_PATH)
