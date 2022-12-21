# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import re
from datetime import datetime, timedelta

from mozperftest.metrics.exceptions import (
    NotebookTransformError,
    NotebookTransformOptionsError,
)

TIME_MATCHER = re.compile(r"(\s+[\d.:]+\s+)")


class LogCatTimeTransformer:
    """Used for parsing times/durations from logcat logs."""

    def open_data(self, file):
        with open(file) as f:
            return f.read()

    def _get_duration(self, startline, endline):
        """Parse duration between two logcat lines.

        Expecting lines with a prefix like:
        05-26 11:45:41.226  ...

        We only parse the hours, minutes, seconds, and milliseconds here
        because we have no use for the days and other times.
        """
        match = TIME_MATCHER.search(startline)
        if not match:
            return None
        start = match.group(1).strip()

        match = TIME_MATCHER.search(endline)
        if not match:
            return None
        end = match.group(1).strip()

        sdt = datetime.strptime(start, "%H:%M:%S.%f")
        edt = datetime.strptime(end, "%H:%M:%S.%f")

        # If the ending is less than the start, we rolled into a new
        # day, so we add 1 day to the end time to handle this
        if sdt > edt:
            edt += timedelta(1)

        return (edt - sdt).total_seconds() * 1000

    def _parse_logcat(self, logcat, first_ts, second_ts=None, processor=None):
        """Parse data from logcat lines.

        If two regexes are provided (first_ts, and second_ts), then the elapsed
        time between those lines will be measured. Otherwise, if only `first_ts`
        is defined then, we expect a number as the first group from the
        match. Optionally, a `processor` function can be provided to process
        all the groups that were obtained from the match, allowing users to
        customize what the result is.

        :param list logcat: The logcat lines to parse.
        :param str first_ts: Regular expression for the first matching line.
        :param str second_ts: Regular expression for the second matching line.
        :param func processor: Function to process the groups from the first_ts
            regular expression.
        :return list: Returns a list of durations/times parsed.
        """
        full_re = r"(" + first_ts + r"\n)"
        if second_ts:
            full_re += r".+(?:\n.+)+?(\n" + second_ts + r"\n)"

        durations = []
        for match in re.findall(full_re, logcat, re.MULTILINE):
            if isinstance(match, str):
                raise NotebookTransformOptionsError(
                    "Only one regex was provided, and it has no groups to process."
                )

            if second_ts is not None:
                if len(match) != 2:
                    raise NotebookTransformError(
                        "More than 2 groups found. It's unclear which "
                        "to use for calculating the durations."
                    )
                val = self._get_duration(match[0], match[1])
            elif processor is not None:
                # Ignore the first match (that is the full line)
                val = processor(match[1:])
            else:
                val = match[1]

            if val is not None:
                durations.append(float(val))

        return durations

    def transform(self, data, **kwargs):
        alltimes = self._parse_logcat(
            data,
            kwargs.get("first-timestamp"),
            second_ts=kwargs.get("second-timestamp"),
            processor=kwargs.get("processor"),
        )
        subtest = kwargs.get("transform-subtest-name")
        return [
            {
                "data": [{"value": val, "xaxis": c} for c, val in enumerate(alltimes)],
                "subtest": subtest if subtest else "logcat-metric",
            }
        ]

    def merge(self, sde):
        grouped_data = {}

        for entry in sde:
            subtest = entry["subtest"]
            data = grouped_data.get(subtest, [])
            data.extend(entry["data"])
            grouped_data.update({subtest: data})

        return [{"data": v, "subtest": k} for k, v in grouped_data.items()]
