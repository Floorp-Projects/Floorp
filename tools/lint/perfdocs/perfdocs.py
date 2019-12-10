# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function

import os
import re


def run_perfdocs(config, logger=None, paths=None, verify=True, generate=False):
    '''
    Build up performance testing documentation dynamically by combining
    text data from YAML files that reside in `perfdoc` folders
    across the `testing` directory. Each directory is expected to have
    an `index.rst` file along with `config.yml` YAMLs defining what needs
    to be added to the documentation.

    The YAML must also define the name of the "framework" that should be
    used in the main index.rst for the performance testing documentation.

    The testing documentation list will be ordered alphabetically once
    it's produced (to avoid unwanted shifts because of unordered dicts
    and path searching).

    Note that the suite name headings will be given the H4 (---) style so it
    is suggested that you use H3 (===) style as the heading for your
    test section. H5 will be used be used for individual tests within each
    suite.

    Usage for verification: ./mach lint -l perfdocs
    Usage for generation: Not Implemented

    Currently, doc generation is not implemented - only validation.

    For validation, see the Verifier class for a description of how
    it works.

    The run will fail if the valid result from validate_tree is not
    False, implying some warning/problem was logged.

    :param dict config: The configuration given by mozlint.
    :param StructuredLogger logger: The StructuredLogger instance to be used to
        output the linting warnings/errors.
    :param list paths: The paths that are being tested. Used to filter
        out errors from files outside of these paths.
    :param bool verify: If true, the verification will be performed.
    :param bool generate: If true, the docs will be generated.
    '''
    from perfdocs.logger import PerfDocLogger

    top_dir = os.environ.get('WORKSPACE', None)
    if not top_dir:
        floc = os.path.abspath(__file__)
        top_dir = floc.split('tools')[0]

    PerfDocLogger.LOGGER = logger
    # Convert all the paths to relative ones
    rel_paths = [re.sub(".*testing", "testing", path) for path in paths]
    PerfDocLogger.PATHS = rel_paths

    # TODO: Expand search to entire tree rather than just the testing directory
    testing_dir = os.path.join(top_dir, 'testing')
    if not os.path.exists(testing_dir):
        raise Exception("Cannot locate testing directory at %s" % testing_dir)

    # Run either the verifier or generator
    if generate:
        raise NotImplementedError
    if verify:
        from perfdocs.verifier import Verifier

        verifier = Verifier(testing_dir, top_dir)
        verifier.validate_tree()
