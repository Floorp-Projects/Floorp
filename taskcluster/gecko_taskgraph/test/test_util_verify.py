#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
There are some basic tests run as part of the Decision task to make sure
documentation exists for taskgraph functionality.
These functions are defined in gecko_taskgraph.generator and call
gecko_taskgraph.util.verify.verify_docs with different parameters to do the
actual checking.
"""


import os.path

import pytest
from mozunit import main

import gecko_taskgraph.util.verify
from gecko_taskgraph import GECKO
from gecko_taskgraph.util.verify import DocPaths, verify_docs

FF_DOCS_BASE = os.path.join(GECKO, "taskcluster", "docs")
EXTRA_DOCS_BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), "docs"))


@pytest.fixture
def mock_single_doc_path(monkeypatch):
    """Set a single path containing documentation"""
    mocked_documentation_paths = DocPaths()
    mocked_documentation_paths.add(FF_DOCS_BASE)
    monkeypatch.setattr(
        gecko_taskgraph.util.verify, "documentation_paths", mocked_documentation_paths
    )


@pytest.fixture
def mock_two_doc_paths(monkeypatch):
    """Set two paths containing documentation"""
    mocked_documentation_paths = DocPaths()
    mocked_documentation_paths.add(FF_DOCS_BASE)
    mocked_documentation_paths.add(EXTRA_DOCS_BASE)
    monkeypatch.setattr(
        gecko_taskgraph.util.verify, "documentation_paths", mocked_documentation_paths
    )


@pytest.mark.usefixtures("mock_single_doc_path")
class PyTestSingleDocPath:
    """
    Taskcluster documentation for Firefox is in a single directory. Check the tests
    running at build time to make sure documentation exists, actually work themselves.
    """

    def test_heading(self):
        """
        Look for a headings in filename matching identifiers. This is used when making sure
        documentation exists for kinds and attributes.
        """
        verify_docs(
            filename="kinds.rst",
            identifiers=["build", "packages", "toolchain"],
            appearing_as="heading",
        )
        with pytest.raises(Exception, match="missing from doc file"):
            verify_docs(
                filename="kinds.rst",
                identifiers=["build", "packages", "badvalue"],
                appearing_as="heading",
            )

    def test_inline_literal(self):
        """
        Look for inline-literals in filename. Used when checking documentation for decision
        task parameters and run-using functions.
        """
        verify_docs(
            filename="parameters.rst",
            identifiers=["base_repository", "head_repository", "owner"],
            appearing_as="inline-literal",
        )
        with pytest.raises(Exception, match="missing from doc file"):
            verify_docs(
                filename="parameters.rst",
                identifiers=["base_repository", "head_repository", "badvalue"],
                appearing_as="inline-literal",
            )


@pytest.mark.usefixtures("mock_two_doc_paths")
class PyTestTwoDocPaths:
    """
    Thunderbird extends Firefox's taskgraph with additional kinds. The documentation
    for Thunderbird kinds are in its repository, and documentation_paths will have
    two places to look for files. Run the same tests as for a single documentation
    path, and cover additional possible scenarios.
    """

    def test_heading(self):
        """
        Look for a headings in filename matching identifiers. This is used when
        making sure documentation exists for kinds and attributes.
        The first test looks for headings that are all within the first doc path,
        the second test is new and has a heading found in the second path.
        The final check has a identifier that will not match and should
        produce an error.
        """
        verify_docs(
            filename="kinds.rst",
            identifiers=["build", "packages", "toolchain"],
            appearing_as="heading",
        )
        verify_docs(
            filename="kinds.rst",
            identifiers=["build", "packages", "newkind"],
            appearing_as="heading",
        )
        with pytest.raises(Exception, match="missing from doc file"):
            verify_docs(
                filename="kinds.rst",
                identifiers=["build", "packages", "badvalue"],
                appearing_as="heading",
            )

    def test_inline_literal(self):
        """
        Look for inline-literals in filename. Used when checking documentation for decision
        task parameters and run-using functions. As with the heading tests,
        the second check looks for an identifier in the added documentation path.
        """
        verify_docs(
            filename="parameters.rst",
            identifiers=["base_repository", "head_repository", "owner"],
            appearing_as="inline-literal",
        )
        verify_docs(
            filename="parameters.rst",
            identifiers=["base_repository", "head_repository", "newparameter"],
            appearing_as="inline-literal",
        )
        with pytest.raises(Exception, match="missing from doc file"):
            verify_docs(
                filename="parameters.rst",
                identifiers=["base_repository", "head_repository", "badvalue"],
                appearing_as="inline-literal",
            )


if __name__ == "__main__":
    main()
