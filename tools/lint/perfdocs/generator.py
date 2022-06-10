# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os
import re
import shutil
import tempfile

from perfdocs.logger import PerfDocLogger
from perfdocs.utils import are_dirs_equal, ON_TRY, read_file, read_yaml, save_file

logger = PerfDocLogger()


class Generator(object):
    """
    After each perfdocs directory was validated, the generator uses the templates
    for each framework, fills them with the test descriptions in config and saves
    the perfdocs in the form index.rst as index file and suite_name.rst for
    each suite of tests in the framework.
    """

    def __init__(self, verifier, workspace, generate=False):
        """
        Initialize the Generator.

        :param verifier: Verifier object. It should not be a fresh Verifier object,
        but an initialized one with validate_tree() method already called
        :param workspace: Path to the top-level checkout directory.
        :param generate: Flag for generating the documentation
        """
        self._workspace = workspace
        if not self._workspace:
            raise Exception("PerfDocs Generator requires a workspace directory.")
        # Template documents without added information reside here
        self.templates_path = os.path.join(
            self._workspace, "tools", "lint", "perfdocs", "templates"
        )
        self.perfdocs_path = os.path.join(
            self._workspace, "testing", "perfdocs", "generated"
        )

        self._generate = generate
        self._verifier = verifier
        self._perfdocs_tree = self._verifier._gatherer.perfdocs_tree

    def build_perfdocs_from_tree(self):
        """
        Builds up a document for each framework that was found.

        :return dict: A dictionary containing a mapping from each framework
            to the document that was built for it, i.e:
            {
                framework_name: framework_document,
                ...
            }
        """

        # Using the verified `perfdocs_tree`, build up the documentation.
        frameworks_info = {}
        for framework in self._perfdocs_tree:
            yaml_content = read_yaml(os.path.join(framework["path"], framework["yml"]))
            rst_content = read_file(
                os.path.join(framework["path"], framework["rst"]), stringify=True
            )

            # Gather all tests and descriptions and format them into
            # documentation content
            documentation = []
            suites = yaml_content["suites"]
            for suite_name in sorted(suites.keys()):
                suite_info = suites[suite_name]

                # Add the suite section
                documentation.extend(
                    self._verifier._gatherer.framework_gatherers[
                        yaml_content["name"]
                    ].build_suite_section(suite_name, suite_info["description"])
                )

                tests = suite_info.get("tests", {})
                for test_name in sorted(tests.keys()):
                    gatherer = self._verifier._gatherer.framework_gatherers[
                        yaml_content["name"]
                    ]
                    test_description = gatherer.build_test_description(
                        test_name, tests[test_name], suite_name
                    )
                    documentation.extend(test_description)
                documentation.append("")

            # Insert documentation into `.rst` file
            framework_rst = re.sub(
                r"{documentation}", "\n".join(documentation), rst_content
            )
            frameworks_info[yaml_content["name"]] = {
                "dynamic": framework_rst,
                "static": [],
            }

            # For static `.rst` file
            for static_file in framework["static"]:
                frameworks_info[yaml_content["name"]]["static"].append(
                    {
                        "file": static_file,
                        "content": read_file(
                            os.path.join(framework["path"], static_file), stringify=True
                        ),
                    }
                )

        return frameworks_info

    def _create_temp_dir(self):
        """
        Create a temp directory as preparation of saving the documentation tree.
        :return: str the location of perfdocs_tmpdir
        """
        # Build the directory that will contain the final result (a tmp dir
        # that will be moved to the final location afterwards)
        try:
            tmpdir = tempfile.mkdtemp()
            perfdocs_tmpdir = os.path.join(tmpdir, "generated")
            os.mkdir(perfdocs_tmpdir)
        except OSError as e:
            logger.critical("Error creating temp file: {}".format(e))

        success = False or os.path.isdir(perfdocs_tmpdir)
        if success:
            return perfdocs_tmpdir
        else:
            return success

    def _create_perfdocs(self):
        """
        Creates the perfdocs documentation.
        :return: str path of the temp dir it is saved in
        """
        # All directories that are kept in the perfdocs tree are valid,
        # so use it to build up the documentation.
        framework_docs = self.build_perfdocs_from_tree()
        perfdocs_tmpdir = self._create_temp_dir()

        # Save the documentation files
        frameworks = []
        for framework_name in sorted(framework_docs.keys()):
            frameworks.append(framework_name)
            save_file(
                framework_docs[framework_name]["dynamic"],
                os.path.join(perfdocs_tmpdir, framework_name),
            )

            for static_name in framework_docs[framework_name]["static"]:
                save_file(
                    static_name["content"],
                    os.path.join(perfdocs_tmpdir, static_name["file"].split(".")[0]),
                )

        # Get the main page and add the framework links to it
        mainpage = read_file(
            os.path.join(self.templates_path, "index.rst"), stringify=True
        )

        fmt_frameworks = "\n".join(["  * :doc:`%s`" % name for name in frameworks])
        fmt_toctree = "\n".join(["  %s" % name for name in frameworks])

        fmt_mainpage = re.sub(r"{toctree_documentation}", fmt_toctree, mainpage)
        fmt_mainpage = re.sub(r"{test_documentation}", fmt_frameworks, fmt_mainpage)

        save_file(fmt_mainpage, os.path.join(perfdocs_tmpdir, "index"))

        return perfdocs_tmpdir

    def _save_perfdocs(self, perfdocs_tmpdir):
        """
        Copies the perfdocs tree after it was saved into the perfdocs_tmpdir
        :param perfdocs_tmpdir: str location of the temp dir where the
        perfdocs was saved
        """
        # Remove the old docs and copy the new version there without
        # checking if they need to be regenerated.
        logger.log("Regenerating perfdocs...")

        if os.path.exists(self.perfdocs_path):
            shutil.rmtree(self.perfdocs_path)

        try:
            saved = shutil.copytree(perfdocs_tmpdir, self.perfdocs_path)
            if saved:
                logger.log(
                    "Documentation saved to {}/".format(
                        re.sub(".*testing", "testing", self.perfdocs_path)
                    )
                )
        except Exception as e:
            logger.critical(
                "There was an error while saving the documentation: {}".format(e)
            )

    def generate_perfdocs(self):
        """
        Generate the performance documentation.

        If `self._generate` is True, then the documentation will be regenerated
        without any checks. Otherwise, if it is False, the new documentation will be
        prepare and compare with the existing documentation to determine if
        it should be regenerated.

        :return bool: True/False - For True, if `self._generate` is True, then the
            docs were regenerated. If `self._generate` is False, then True will mean
            that the docs should be regenerated, and False means that they do not
            need to be regenerated.
        """

        def get_possibly_changed_files():
            """
            Returns files that might have been modified
            (used to output a linter warning for regeneration)
            :return: list - files that might have been modified
            """
            # Returns files that might have been modified
            # (used to output a linter warning for regeneration)
            files = []
            for entry in self._perfdocs_tree:
                files.extend(
                    [
                        os.path.join(entry["path"], entry["yml"]),
                        os.path.join(entry["path"], entry["rst"]),
                    ]
                )
            return files

        # Throw a warning if there's no need for generating
        if not os.path.exists(self.perfdocs_path) and not self._generate:
            # If they don't exist and we are not generating, then throw
            # a linting error and exit.
            logger.warning(
                "PerfDocs need to be regenerated.", files=get_possibly_changed_files()
            )
            return True

        perfdocs_tmpdir = self._create_perfdocs()
        if self._generate:
            self._save_perfdocs(perfdocs_tmpdir)
        else:
            # If we are not generating, then at least check if they
            # should be regenerated by comparing the directories.
            if not are_dirs_equal(perfdocs_tmpdir, self.perfdocs_path):
                logger.warning(
                    "PerfDocs are outdated, run ./mach lint -l perfdocs --fix .` "
                    + "to update them. You can also apply the "
                    + f"{'perfdocs.diff' if ON_TRY else 'diff.txt'} patch file "
                    + f"{'produced from this reviewbot test ' if ON_TRY else ''}"
                    + "to fix the issue.",
                    files=get_possibly_changed_files(),
                )
