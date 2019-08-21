# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
import json
import subprocess

from lib.variant import Variant, VariantApk


def get_variant(build_type, engine):
    print("Fetching variant information for build_type='{}', engine='{}'".format(build_type, engine))
    output = _run_gradle_process('printVariant', variantBuildType=build_type, variantEngine=engine)
    content = _extract_content_from_command_output(output, prefix='variant: ')
    raw_variant = json.loads(content)
    return Variant(
        raw_variant['name'],
        build_type,
        [VariantApk(build_type, raw_apk['abi'], engine, raw_apk['fileName']) for raw_apk in raw_variant['apks']]
    )


def _run_gradle_process(gradle_command, **kwargs):
    gradle_properties = [
        '-P{property_name}={value}'.format(property_name=property_name, value=value)
        for property_name, value in kwargs.iteritems()
    ]

    process = subprocess.Popen(["./gradlew", "--no-daemon", "--quiet", gradle_command] + gradle_properties, stdout=subprocess.PIPE)
    output, err = process.communicate()
    exit_code = process.wait()

    if exit_code is not 0:
        print("Gradle command returned error: {}".format(exit_code))

    return output


def _extract_content_from_command_output(output, prefix):
    variants_line = [line for line in output.split('\n') if line.startswith(prefix)][0]
    return variants_line.split(' ', 1)[1]
