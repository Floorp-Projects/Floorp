# coding=utf8
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1523747 - Migrate customizeMode strings to Fluent, part {index}"""
    ctx.maybe_add_localization('browser/chrome/browser/browser.dtd')
    ctx.add_transforms(
        'browser/browser/customizeMode.ftl',
        'browser/browser/customizeMode.ftl',
        transforms_from(
"""
customize-mode-restore-defaults = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.restoreDefaults") }
customize-mode-menu-and-toolbars-header = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.menuAndToolbars.header3") }
customize-mode-overflow-list-title = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.overflowList.title2") }
customize-mode-uidensity = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity") }
customize-mode-done = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.done") }
customize-mode-lwthemes-menu-manage = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.lwthemes.menuManage") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.lwthemes.menuManage.accessKey") }
customize-mode-toolbars = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.toolbars2") }
customize-mode-titlebar = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.titlebar") }
customize-mode-uidensity-menu-touch = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuTouch.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuTouch.accessKey") }
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuTouch.tooltip") }
customize-mode-uidensity-auto-touch-mode-checkbox = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.autoTouchMode.checkbox.label") }
customize-mode-extra-drag-space = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.extraDragSpace") }
customize-mode-lwthemes = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.lwthemes") }
customize-mode-overflow-list-description = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.overflowList.description") }
customize-mode-uidensity-menu-normal = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuNormal.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuNormal.accessKey") }
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuNormal.tooltip") }
customize-mode-uidensity-menu-compact = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuCompact.label") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuCompact.accessKey") }
    .tooltiptext = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.uidensity.menuCompact.tooltip") }
customize-mode-lwthemes-menu-get-more = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.lwthemes.menuGetMore") }
    .accesskey = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.lwthemes.menuGetMore.accessKey") }
customize-mode-undo-cmd = 
    .label = { COPY("browser/chrome/browser/browser.dtd", "undoCmd.label") }
customize-mode-lwthemes-my-themes = 
    .value = { COPY("browser/chrome/browser/browser.dtd", "customizeMode.lwthemes.myThemes") }
"""
        )
    )

