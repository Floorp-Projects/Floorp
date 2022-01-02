# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1743347 - Move whyPaused.* strings to Fluent, part {index}."""

    source = "devtools/client/debugger.properties"
    target = "devtools/shared/debugger-paused-reasons.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("whypaused-debugger-statement"),
                value=COPY(source, "whyPaused.debuggerStatement"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-breakpoint"),
                value=COPY(source, "whyPaused.breakpoint"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-event-breakpoint"),
                value=COPY(source, "whyPaused.eventBreakpoint"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-exception"),
                value=COPY(source, "whyPaused.exception"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-mutation-breakpoint"),
                value=COPY(source, "whyPaused.mutationBreakpoint"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-mutation-breakpoint-added"),
                value=COPY(source, "whyPaused.mutationBreakpointAdded"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-mutation-breakpoint-removed"),
                value=COPY(source, "whyPaused.mutationBreakpointRemoved"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-interrupted"),
                value=COPY(source, "whyPaused.interrupted"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-resume-limit"),
                value=COPY(source, "whyPaused.resumeLimit"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-pause-on-dom-events"),
                value=COPY(source, "whyPaused.pauseOnDOMEvents"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-breakpoint-condition-thrown"),
                value=COPY(source, "whyPaused.breakpointConditionThrown"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-xhr"),
                value=COPY(source, "whyPaused.XHR"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-promise-rejection"),
                value=COPY(source, "whyPaused.promiseRejection"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-get-watchpoint"),
                value=COPY(source, "whyPaused.getWatchpoint"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-set-watchpoint"),
                value=COPY(source, "whyPaused.setWatchpoint"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-assert"),
                value=COPY(source, "whyPaused.assert"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-debug-command"),
                value=COPY(source, "whyPaused.debugCommand"),
            ),
            FTL.Message(
                id=FTL.Identifier("whypaused-other"),
                value=COPY(source, "whyPaused.other"),
            ),
        ],
    )
