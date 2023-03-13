# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
This transform splits the jobs it receives into per-locale tasks. Locales are
provided by the `locales-file`.
"""

from copy import deepcopy
from pprint import pprint

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema
from voluptuous import Extra, Optional, Required

from gecko_taskgraph.transforms.l10n import parse_locales_file

transforms = TransformSequence()

split_by_locale_schema = Schema(
    {
        # The file to pull locale information from. This should be a json file
        # such as browser/locales/l10n-changesets.json.
        Required("locales-file"): str,
        # The platform name in the form used by the locales files. Defaults to
        # attributes.build_platform if not provided.
        Optional("locale-file-platform"): str,
        # A list of properties elsewhere in the job that need to have the locale
        # name substituted into them. The referenced properties may be strings
        # or lists. In the case of the latter, all list values will have
        # substitutions performed.
        Optional("properties-with-locale"): [str],
        Extra: object,
    }
)


transforms.add_validate(split_by_locale_schema)


@transforms.add
def add_command(config, jobs):
    for job in jobs:
        locales_file = job.pop("locales-file")
        properties_with_locale = job.pop("properties-with-locale")
        build_platform = job.pop(
            "locale-file-platform", job["attributes"]["build_platform"]
        )

        for locale in parse_locales_file(locales_file, build_platform):
            locale_job = deepcopy(job)
            locale_job["attributes"]["locale"] = locale
            for prop in properties_with_locale:
                container, subfield = locale_job, prop
                while "." in subfield:
                    f, subfield = subfield.split(".", 1)
                    if f not in container:
                        raise Exception(
                            f"Unable to find property {prop} to perform locale substitution on. Job is:\n{pprint(job)}"
                        )
                    container = container[f]
                    if not isinstance(container, dict):
                        raise Exception(
                            f"{container} is not a dict, cannot perform locale substitution. Job is:\n{pprint(job)}"
                        )

                if isinstance(container[subfield], str):
                    container[subfield] = container[subfield].format(locale=locale)
                elif isinstance(container[subfield], list):
                    for i in range(len(container[subfield])):
                        container[subfield][i] = container[subfield][i].format(
                            locale=locale
                        )
                else:
                    raise Exception(
                        f"Don't know how to subtitute locale for value of type: {type(container[subfield])}; value is: {container[subfield]}"
                    )

            yield locale_job
