# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os

from mozperftest.base import MachEnvironment
from mozperftest.metrics.common import CommonMetrics
from mozperftest.metrics.utils import write_json
from mozperftest.metrics.browsertime import process


class MissingResultsError(Exception):
    pass


KNOWN_FLAVORS = ["script"]
FLAVOR_TO_PROCESSOR = {"script": process, "default": process}


class Perfherder(MachEnvironment):
    def __call__(self, metadata):
        """Processes the given results into a perfherder-formatted data blob.

        If the `--perfherder` flag isn't providec, then the
        results won't be processed into a perfherder-data blob. If the
        flavor is unknown to us, then we assume that it comes from
        browsertime.

        :param results list/dict/str: Results to process.
        :param perfherder bool: True if results should be processed
            into a perfherder-data blob.
        :param flavor str: The flavor that is being processed.
        """
        # XXX work is happening in cwd, we need to define where
        # the artifacts are uploaded?
        # if not perfherder:
        #    return
        flavor = metadata.get("flavor")
        if not flavor or flavor not in KNOWN_FLAVORS:
            flavor = "default"
            self.warning(
                "Unknown flavor {} was given; we don't know how to process "
                "its results. Attempting with default browsertime processing...".format(
                    flavor
                )
            )

        # Get the common requirements for metrics (i.e. output path,
        # results to process)
        cm = CommonMetrics(metadata["results"], **metadata["mach_args"])

        # Process the results and save them
        # TODO: Get app/browser name from metadata/kwargs
        proc = FLAVOR_TO_PROCESSOR[flavor](cm.results, self.info, app="firefox")

        file = "perfherder-data.json"
        if cm.prefix:
            file = "{}-{}".format(cm.prefix, file)
        self.info(
            "Writing perfherder results to {}".format(os.path.join(cm.output, file))
        )
        metadata["output"] = write_json(proc, cm.output, file)
        return metadata
