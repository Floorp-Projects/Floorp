# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE


def migrate(ctx):
    """Bug 1791178 - Convert DownloadUtils.jsm to Fluent, part {index}."""

    source = "toolkit/chrome/mozapps/downloads/downloads.properties"
    target = "toolkit/toolkit/downloads/downloadUtils.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("download-utils-short-seconds"),
                value=PLURALS(source, "shortSeconds", VARIABLE_REFERENCE("timeValue")),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-short-minutes"),
                value=PLURALS(source, "shortMinutes", VARIABLE_REFERENCE("timeValue")),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-short-hours"),
                value=PLURALS(source, "shortHours", VARIABLE_REFERENCE("timeValue")),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-short-days"),
                value=PLURALS(source, "shortDays", VARIABLE_REFERENCE("timeValue")),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-status"),
                value=REPLACE(
                    source,
                    "statusFormat3",
                    {
                        "%4$S": VARIABLE_REFERENCE("timeLeft"),
                        "%1$S": VARIABLE_REFERENCE("transfer"),
                        "%2$S": VARIABLE_REFERENCE("rate"),
                        "%3$S": VARIABLE_REFERENCE("unit"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-status-infinite-rate"),
                value=REPLACE(
                    source,
                    "statusFormatInfiniteRate",
                    {
                        "%3$S": VARIABLE_REFERENCE("timeLeft"),
                        "%1$S": VARIABLE_REFERENCE("transfer"),
                        "%2$S": COPY(source, "infiniteRate"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-status-no-rate"),
                value=REPLACE(
                    source,
                    "statusFormatNoRate",
                    {
                        "%2$S": VARIABLE_REFERENCE("timeLeft"),
                        "%1$S": VARIABLE_REFERENCE("transfer"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-bytes"), value=COPY(source, "bytes")
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-kilobyte"),
                value=COPY(source, "kilobyte"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-megabyte"),
                value=COPY(source, "megabyte"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-gigabyte"),
                value=COPY(source, "gigabyte"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-transfer-same-units"),
                value=REPLACE(
                    source,
                    "transferSameUnits2",
                    {
                        "%1$S": VARIABLE_REFERENCE("progress"),
                        "%2$S": VARIABLE_REFERENCE("total"),
                        "%3$S": VARIABLE_REFERENCE("totalUnits"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-transfer-diff-units"),
                value=REPLACE(
                    source,
                    "transferDiffUnits2",
                    {
                        "%1$S": VARIABLE_REFERENCE("progress"),
                        "%2$S": VARIABLE_REFERENCE("progressUnits"),
                        "%3$S": VARIABLE_REFERENCE("total"),
                        "%4$S": VARIABLE_REFERENCE("totalUnits"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-transfer-no-total"),
                value=REPLACE(
                    source,
                    "transferNoTotal2",
                    {
                        "%1$S": VARIABLE_REFERENCE("progress"),
                        "%2$S": VARIABLE_REFERENCE("progressUnits"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-time-pair"),
                value=REPLACE(
                    source,
                    "timePair3",
                    {
                        "%1$S": VARIABLE_REFERENCE("time"),
                        "%2$S": VARIABLE_REFERENCE("unit"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-time-left-single"),
                value=REPLACE(
                    source, "timeLeftSingle3", {"%1$S": VARIABLE_REFERENCE("time")}
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-time-left-double"),
                value=REPLACE(
                    source,
                    "timeLeftDouble3",
                    {
                        "%1$S": VARIABLE_REFERENCE("time1"),
                        "%2$S": VARIABLE_REFERENCE("time2"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-time-few-seconds"),
                value=COPY(source, "timeFewSeconds2"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-time-unknown"),
                value=COPY(source, "timeUnknown2"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-done-scheme"),
                value=REPLACE(
                    source, "doneScheme2", {"%1$S": VARIABLE_REFERENCE("scheme")}
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-done-file-scheme"),
                value=COPY(source, "doneFileScheme"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-utils-yesterday"),
                value=COPY(source, "yesterday"),
            ),
        ],
    )
