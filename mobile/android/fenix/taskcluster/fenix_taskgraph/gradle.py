# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import subprocess

from taskgraph.util.memoize import memoize



def get_variant(build_type):
    all_variants = _fetch_all_variants()
    matching_variants = [
        variant for variant in all_variants
        if variant["build_type"] == build_type
    ]
    number_of_matching_variants = len(matching_variants)
    if number_of_matching_variants == 0:
        raise ValueError('No variant found for build type "{}"'.format(
            build_type
        ))
    elif number_of_matching_variants > 1:
        raise ValueError('Too many variants found for build type "{}"": {}'.format(
            build_type, matching_variants
        ))

    return matching_variants.pop()


@memoize
def _fetch_all_variants():
    output = _run_gradle_process('printVariants')
    content = _extract_content_from_command_output(output, prefix='variants: ')
    return json.loads(content)


def _run_gradle_process(gradle_command, **kwargs):
    gradle_properties = [
        '-P{property_name}={value}'.format(property_name=property_name, value=value)
        for property_name, value in kwargs.items()
    ]

    process = subprocess.Popen(["./gradlew", "--no-daemon", "--quiet", gradle_command] + gradle_properties, stdout=subprocess.PIPE, universal_newlines=True)
    output, err = process.communicate()
    exit_code = process.wait()

    if exit_code != 0:
        raise RuntimeError("Gradle command returned error: {}".format(exit_code))

    return output


def _extract_content_from_command_output(output, prefix):
    variants_line = [line for line in output.split('\n') if line.startswith(prefix)][0]
    return variants_line.split(' ', 1)[1]
