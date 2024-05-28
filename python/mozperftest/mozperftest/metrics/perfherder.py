# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import pathlib
import statistics
import sys

import jsonschema

from mozperftest.layers import Layer
from mozperftest.metrics.common import COMMON_ARGS, filtered_metrics
from mozperftest.metrics.exceptions import PerfherderValidDataError
from mozperftest.metrics.notebook.constant import Constant
from mozperftest.metrics.notebook.transformer import get_transformer
from mozperftest.metrics.utils import has_callable_method, is_number, write_json
from mozperftest.utils import strtobool

PERFHERDER_SCHEMA = pathlib.Path(
    "testing", "mozharness", "external_tools", "performance-artifact-schema.json"
)


class Perfherder(Layer):
    """Output data in the perfherder format."""

    name = "perfherder"
    activated = False

    arguments = COMMON_ARGS
    arguments.update(
        {
            "stats": {
                "action": "store_true",
                "default": False,
                "help": "If set, browsertime statistics will be reported.",
            },
            "timestamp": {
                "type": float,
                "default": None,
                "help": (
                    "Timestamp to use for the perfherder data. Can be the "
                    "current date or a past date if needed."
                ),
            },
        }
    )

    def run(self, metadata):
        """Processes the given results into a perfherder-formatted data blob.

        If the `--perfherder` flag isn't provided, then the
        results won't be processed into a perfherder-data blob. If the
        flavor is unknown to us, then we assume that it comes from
        browsertime.

        XXX If needed, make a way to do flavor-specific processing

        :param results list/dict/str: Results to process.
        :param perfherder bool: True if results should be processed
            into a perfherder-data blob.
        :param flavor str: The flavor that is being processed.
        """
        prefix = self.get_arg("prefix")
        output = self.get_arg("output")

        # XXX Make an arugment for exclusions from metrics
        # (or go directly to regex's for metrics)
        exclusions = None
        if not self.get_arg("stats"):
            exclusions = ["statistics."]

        # Get filtered metrics
        metrics = self.get_arg("metrics")
        results, fullsettings = filtered_metrics(
            metadata,
            output,
            prefix,
            metrics=metrics,
            transformer=self.get_arg("transformer"),
            settings=True,
            exclude=exclusions,
            split_by=self.get_arg("split-by"),
            simplify_names=self.get_arg("simplify-names"),
            simplify_exclude=self.get_arg("simplify-exclude"),
        )

        if not any([results[name] for name in results]):
            self.warning("No results left after filtering")
            return metadata

        app_info = {"name": self.get_arg("app", default="firefox")}
        if metadata.binary_version:
            app_info["version"] = metadata.binary_version

        # converting the metrics list into a mapping where
        # keys are the metrics nane
        if metrics is not None:
            metrics = dict([(m["name"], m) for m in metrics])
        else:
            metrics = {}

        all_perfherder_data = None
        for name, res in results.items():
            settings = dict(fullsettings[name])
            # updating the settings with values provided in metrics, if any
            if name in metrics:
                settings.update(metrics[name])

            # XXX Instead of just passing replicates here, we should build
            # up a partial perfherder data blob (with options) and subtest
            # overall values.
            subtests = []
            for r in res:
                subtest = {}
                vals = [v["value"] for v in r["data"] if is_number(v["value"])]
                if vals:
                    subtest["name"] = r["subtest"]
                    subtest["replicates"] = vals
                    subtest["result"] = r
                    subtests.append(subtest)

            perfherder_data = self._build_blob(
                subtests,
                name=name,
                extra_options=settings.get("extraOptions"),
                should_alert=strtobool(settings.get("shouldAlert", False)),
                application=app_info,
                alert_threshold=float(settings.get("alertThreshold", 2.0)),
                lower_is_better=strtobool(settings.get("lowerIsBetter", True)),
                unit=settings.get("unit", "ms"),
                summary=settings.get("value"),
                framework=settings.get("framework"),
                metrics_info=metrics,
                transformer=res[0].get("transformer", None),
            )

            if all_perfherder_data is None:
                all_perfherder_data = perfherder_data
            else:
                all_perfherder_data["suites"].extend(perfherder_data["suites"])

        if prefix:
            # If a prefix was given, store it in the perfherder data as well
            all_perfherder_data["prefix"] = prefix

        timestamp = self.get_arg("timestamp")
        if timestamp is not None:
            all_perfherder_data["pushTimestamp"] = timestamp

        # Validate the final perfherder data blob
        with pathlib.Path(metadata._mach_cmd.topsrcdir, PERFHERDER_SCHEMA).open() as f:
            schema = json.load(f)
        jsonschema.validate(all_perfherder_data, schema)

        file = "perfherder-data.json"
        if prefix:
            file = "{}-{}".format(prefix, file)
        self.info("Writing perfherder results to {}".format(os.path.join(output, file)))

        # XXX "suites" key error occurs when using self.info so a print
        # is being done for now.

        # print() will produce a BlockingIOError on large outputs, so we use
        # sys.stdout
        sys.stdout.write("PERFHERDER_DATA: ")
        json.dump(all_perfherder_data, sys.stdout)
        sys.stdout.write("\n")
        sys.stdout.flush()

        metadata.set_output(write_json(all_perfherder_data, output, file))
        return metadata

    def _build_blob(
        self,
        subtests,
        name="browsertime",
        test_type="pageload",
        extra_options=None,
        should_alert=False,
        subtest_should_alert=None,
        suiteshould_alert=False,
        framework=None,
        application=None,
        alert_threshold=2.0,
        lower_is_better=True,
        unit="ms",
        summary=None,
        metrics_info=None,
        transformer=None,
    ):
        """Build a PerfHerder data blob from the given subtests.

        NOTE: This is a WIP, see the many TODOs across this file.

        Given a dictionary of subtests, and the values. Build up a
        perfherder data blob. Note that the naming convention for
        these arguments is different then the rest of the scripts
        to make it easier to see where they are going to in the perfherder
        data.

        For the `should_alert` field, if should_alert is True but `subtest_should_alert`
        is empty, then all subtests along with the suite will generate alerts.
        Otherwise, if the subtest_should_alert contains subtests to alert on, then
        only those will alert and nothing else (including the suite). If the
        suite value should alert, then set `suiteshould_alert` to True.

        :param subtests dict: A dictionary of subtests and the values.
            XXX TODO items for subtests:
                (1) Allow it to contain replicates and individual settings
                    for each of the subtests.
                (2) The geomean of the replicates will be taken for now,
                    but it should be made more flexible in some way.
                (3) We need some way to handle making multiple suites.
        :param name str: Name to give to the suite.
        :param test_type str: The type of test that was run.
        :param extra_options list: A list of extra options to store.
        :param should_alert bool: Whether all values in the suite should
            generate alerts or not.
        :param subtest_should_alert list: A list of subtests to alert on. If this
            is not empty, then it will disable the suite-level alerts.
        :param suiteshould_alert bool: Used if `subtest_should_alert` is not
            empty, and if True, then the suite-level value will generate
            alerts.
        :param framework dict: Information about the framework that
            is being tested.
        :param application dict: Information about the application that
            is being tested. Must include name, and optionally a version.
        :param alert_threshold float: The change in percentage this
            metric must undergo to to generate an alert.
        :param lower_is_better bool: If True, then lower values are better
            than higher ones.
        :param unit str: The unit of the data.
        :param summary float: The summary value to use in the perfherder
            data blob. By default, the mean of all the subtests will be
            used.
        :param metrics_info dict: Contains a mapping of metric names to the
            options that are used on the metric.
        :param transformer str: The name of a predefined tranformer, a module
            path to a transform, or a path to the file containing the transformer.

        :return dict: The PerfHerder data blob.
        """
        if extra_options is None:
            extra_options = []
        if subtest_should_alert is None:
            subtest_should_alert = []
        if framework is None:
            framework = {"name": "mozperftest"}
        if application is None:
            application = {"name": "firefox", "version": "9000"}
        if metrics_info is None:
            metrics_info = {}

        # Use the transform to produce a suite value
        const = Constant()
        tfm_cls = None
        transformer_obj = None
        if transformer and transformer in const.predefined_transformers:
            # A pre-built transformer name was given
            tfm_cls = const.predefined_transformers[transformer]
            transformer_obj = tfm_cls()
        elif transformer is not None:
            tfm_cls = get_transformer(transformer)
            transformer_obj = tfm_cls()
        else:
            self.warning(
                "No transformer found for this suite. Cannot produce a summary value."
            )

        perf_subtests = []
        suite = {
            "name": name,
            "type": test_type,
            "unit": unit,
            "extraOptions": extra_options,
            "lowerIsBetter": lower_is_better,
            "alertThreshold": alert_threshold,
            "shouldAlert": (should_alert and not subtest_should_alert)
            or suiteshould_alert,
            "subtests": perf_subtests,
        }

        perfherder = {
            "suites": [suite],
            "framework": framework,
            "application": application,
        }

        allvals = []
        alert_thresholds = []
        for subtest_res in subtests:
            measurement = subtest_res["name"]
            reps = subtest_res["replicates"]
            extra_info = subtest_res["result"]
            allvals.extend(reps)

            if len(reps) == 0:
                self.warning("No replicates found for {}, skipping".format(measurement))
                continue

            # Gather extra settings specified from within a metric specification
            subtest_lower_is_better = lower_is_better
            subtest_unit = unit
            for met in metrics_info:
                if met not in measurement:
                    continue

                extra_options.extend(metrics_info[met].get("extraOptions", []))
                alert_thresholds.append(
                    metrics_info[met].get("alertThreshold", alert_threshold)
                )

                subtest_unit = metrics_info[met].get("unit", unit)
                subtest_lower_is_better = metrics_info[met].get(
                    "lowerIsBetter", lower_is_better
                )

                if metrics_info[met].get("shouldAlert", should_alert):
                    subtest_should_alert.append(measurement)

                break

            subtest = {
                "name": measurement,
                "replicates": reps,
                "lowerIsBetter": extra_info.get(
                    "lowerIsBetter", subtest_lower_is_better
                ),
                "value": None,
                "unit": extra_info.get("unit", subtest_unit),
                "shouldAlert": extra_info.get(
                    "shouldAlert", should_alert or measurement in subtest_should_alert
                ),
            }

            if has_callable_method(transformer_obj, "subtest_summary"):
                subtest["value"] = transformer_obj.subtest_summary(subtest)
            if subtest["value"] is None:
                subtest["value"] = statistics.mean(reps)

            perf_subtests.append(subtest)

        if len(allvals) == 0:
            raise PerfherderValidDataError(
                "Could not build perfherder data blob because no valid data was provided, "
                + "only int/float data is accepted."
            )

        alert_thresholds = list(set(alert_thresholds))
        if len(alert_thresholds) > 1:
            raise PerfherderValidDataError(
                "Too many alertThreshold's were specified, expecting 1 but found "
                + f"{len(alert_thresholds)}"
            )
        elif len(alert_thresholds) == 1:
            suite["alertThreshold"] = alert_thresholds[0]

        suite["extraOptions"] = list(set(suite["extraOptions"]))

        if has_callable_method(transformer_obj, "summary"):
            val = transformer_obj.summary(suite)
            if val is not None:
                suite["value"] = val

        return perfherder
