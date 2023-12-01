# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1866268 - Convert GeckoViewConsole strings to Fluent, part {index}."""

    source = "mobile/android/chrome/browser.properties"
    target = "mobile/android/mobile/android/geckoViewConsole.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("console-stacktrace-anonymous-function"),
                value=COPY(source, "stacktrace.anonymousFunction"),
            ),
            FTL.Message(
                id=FTL.Identifier("console-stacktrace"),
                value=REPLACE(
                    source,
                    "stacktrace.outputMessage",
                    {
                        "%1$S": VARIABLE_REFERENCE("filename"),
                        "%2$S": VARIABLE_REFERENCE("functionName"),
                        "%3$S": VARIABLE_REFERENCE("lineNumber"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("console-timer-start"),
                value=REPLACE(
                    source, "timer.start", {"%1$S": VARIABLE_REFERENCE("name")}
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("console-timer-end"),
                value=REPLACE(
                    source,
                    "timer.end",
                    {
                        "%1$S": VARIABLE_REFERENCE("name"),
                        "%2$S": VARIABLE_REFERENCE("duration"),
                    },
                ),
            ),
        ],
    )
