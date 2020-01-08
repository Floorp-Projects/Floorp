# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY

def migrate(ctx):
    """Bug 1592043 - Migrate toolbox options strings from DTD to FTL, part {index}"""

    ctx.add_transforms(
        "devtools/client/toolbox-options.ftl",
        "devtools/client/toolbox-options.ftl",
        transforms_from(
            """

options-select-default-tools-label = { COPY(path1, "options.selectDefaultTools.label2") }

options-tool-not-supported-label = { COPY(path1, "options.toolNotSupported.label") }

options-select-additional-tools-label = { COPY(path1, "options.selectAdditionalTools.label") }

options-select-enabled-toolbox-buttons-label = { COPY(path1, "options.selectEnabledToolboxButtons.label") }

options-select-dev-tools-theme-label = { COPY(path1, "options.selectDevToolsTheme.label2") }

options-context-inspector = { COPY(path1, "options.context.inspector") }

options-show-user-agent-styles-tooltip =
    .title = { COPY(path1, "options.showUserAgentStyles.tooltip") }
options-show-user-agent-styles-label = { COPY(path1, "options.showUserAgentStyles.label") }

options-collapse-attrs-tooltip =
    .title = { COPY(path1, "options.collapseAttrs.tooltip") }
options-collapse-attrs-label = { COPY(path1, "options.collapseAttrs.label") }

options-default-color-unit-label = { COPY(path1, "options.defaultColorUnit.label") }

options-default-color-unit-authored = { COPY(path1, "options.defaultColorUnit.authored") }

options-default-color-unit-hex = { COPY(path1, "options.defaultColorUnit.hex") }

options-default-color-unit-hsl = { COPY(path1, "options.defaultColorUnit.hsl") }

options-default-color-unit-rgb = { COPY(path1, "options.defaultColorUnit.rgb") }

options-default-color-unit-name = { COPY(path1, "options.defaultColorUnit.name") }

options-debugger-label = { COPY(path1, "options.debugger.label") }

options-styleeditor-label = { COPY(path1, "options.styleeditor.label") }

options-stylesheet-autocompletion-tooltip =
    .title = { COPY(path1, "options.stylesheetAutocompletion.tooltip") }
options-stylesheet-autocompletion-label = { COPY(path1, "options.stylesheetAutocompletion.label") }

options-screenshot-label = { COPY(path1, "options.screenshot.label") }

options-screenshot-clipboard-tooltip =
    .title = { COPY(path1, "options.screenshot.clipboard.tooltip") }
options-screenshot-clipboard-label = { COPY(path1, "options.screenshot.clipboard.label") }

options-screenshot-audio-tooltip =
    .title = { COPY(path1, "options.screenshot.audio.tooltip") }
options-screenshot-audio-label = { COPY(path1, "options.screenshot.audio.label") }

options-sourceeditor-label = { COPY(path1, "options.sourceeditor.label") }

options-sourceeditor-detectindentation-tooltip =
    .title = { COPY(path1, "options.sourceeditor.detectindentation.tooltip") }
options-sourceeditor-detectindentation-label = { COPY(path1, "options.sourceeditor.detectindentation.label") }

options-sourceeditor-autoclosebrackets-tooltip =
    .title = { COPY(path1, "options.sourceeditor.autoclosebrackets.tooltip") }
options-sourceeditor-autoclosebrackets-label = { COPY(path1, "options.sourceeditor.autoclosebrackets.label") }

options-sourceeditor-expandtab-tooltip =
    .title = { COPY(path1, "options.sourceeditor.expandtab.tooltip") }
options-sourceeditor-expandtab-label = { COPY(path1, "options.sourceeditor.expandtab.label") }

options-sourceeditor-tabsize-label = { COPY(path1, "options.sourceeditor.tabsize.label") }

options-sourceeditor-keybinding-label = { COPY(path1, "options.sourceeditor.keybinding.label") }

options-sourceeditor-keybinding-default-label = { COPY(path1, "options.sourceeditor.keybinding.default.label") }

options-context-advanced-settings = { COPY(path1, "options.context.advancedSettings") }

options-source-maps-tooltip =
    .title = { COPY(path1, "options.sourceMaps.tooltip1") }
options-source-maps-label = { COPY(path1, "options.sourceMaps.label") }

options-show-platform-data-tooltip =
    .title = { COPY(path1, "options.showPlatformData.tooltip") }
options-show-platform-data-label = { COPY(path1, "options.showPlatformData.label") }

options-disable-http-cache-tooltip =
    .title = { COPY(path1, "options.disableHTTPCache.tooltip") }
options-disable-http-cache-label = { COPY(path1, "options.disableHTTPCache.label") }

options-disable-javascript-tooltip =
    .title = { COPY(path1, "options.disableJavaScript.tooltip") }
options-disable-javascript-label = { COPY(path1, "options.disableJavaScript.label") }

options-enable-service-workers-http-tooltip =
    .title = { COPY(path1, "options.enableServiceWorkersHTTP.tooltip") }
options-enable-service-workers-http-label = { COPY(path1, "options.enableServiceWorkersHTTP.label") }

options-enable-chrome-tooltip =
    .title = { COPY(path1, "options.enableChrome.tooltip3") }
options-enable-chrome-label = { COPY(path1, "options.enableChrome.label5") }

options-enable-remote-tooltip =
    .title = { COPY(path1, "options.enableRemote.tooltip2") }
options-enable-remote-label = { COPY(path1, "options.enableRemote.label3") }

options-context-triggers-page-refresh = { COPY(path1, "options.context.triggersPageRefresh") }

""", path1="devtools/client/toolbox.dtd"))
