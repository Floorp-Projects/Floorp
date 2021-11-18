# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1733496 - Convert key-shortcuts.properties to Fluent, part {index}."""

    source = "devtools/startup/key-shortcuts.properties"
    target = "devtools/startup/key-shortcuts.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-toggle-toolbox"),
                value=COPY(source, "toggleToolbox.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-toggle-toolbox-f12"),
                value=COPY(source, "toggleToolboxF12.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-browser-toolbox"),
                value=COPY(source, "browserToolbox.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-browser-console"),
                value=COPY(source, "browserConsole.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-responsive-design-mode"),
                value=COPY(source, "responsiveDesignMode.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-inspector"),
                value=COPY(source, "inspector.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-webconsole"),
                value=COPY(source, "webconsole.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-jsdebugger"),
                value=COPY(source, "jsdebugger.commandkey2"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-netmonitor"),
                value=COPY(source, "netmonitor.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-styleeditor"),
                value=COPY(source, "styleeditor.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-performance"),
                value=COPY(source, "performance.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-storage"),
                value=COPY(source, "storage.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-dom"),
                value=COPY(source, "dom.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-accessibility-f12"),
                value=COPY(source, "accessibilityF12.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-profiler-start-stop"),
                value=COPY(source, "profilerStartStop.commandkey"),
            ),
            FTL.Message(
                id=FTL.Identifier("devtools-commandkey-profiler-capture"),
                value=COPY(source, "profilerCapture.commandkey"),
            ),
        ],
    )
