# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import REPLACE, CONCAT, COPY
from fluent.migrate.helpers import VARIABLE_REFERENCE

# This helper deals with migrating a set of DTD strings  used along the lines of
# <parent-el>&before; <span>content</span> &after;</parent-el>
# such that the resulting fluent strings take the forms:
# ... = <span>content</span> after
# ... = before <span>content</span>
# ... = before <span>content</span> after
# ie such that there is a space before/after the middle (ie <span>) element
# depending on whether it is followed by the empty string or by some other
# bit of content.
class CONCAT_BEFORE_AFTER(CONCAT):
    def __call__(self, ctx):
        assert len(self.elements) == 3
        pattern_before, middle, pattern_after = self.elements
        elem_before = pattern_before.elements[0]
        elem_after = pattern_after.elements[0]

        if isinstance(elem_before, FTL.TextElement) and \
           len(elem_before.value) > 0 and \
           elem_before.value[-1] != " ":
            elem_before.value += " "
        if isinstance(elem_after, FTL.TextElement) and \
           len(elem_after.value) > 0 and \
           elem_after.value[0] != " ":
            elem_after.value = " " + elem_after.value
        return super(CONCAT_BEFORE_AFTER, self).__call__(ctx)

def migrate(ctx):
    """Bug 1663879 - convert full screen and pointer lock warnings to fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("fullscreen-warning-domain"),
                value=CONCAT_BEFORE_AFTER(
                    COPY(
                        "browser/chrome/browser/browser.dtd",
                        "fullscreenWarning.beforeDomain.label",
                        trim=True
                        ),
                    CONCAT(
                        FTL.TextElement('<span data-l10n-name="domain">'),
                        VARIABLE_REFERENCE('domain'),
                        FTL.TextElement('</span>'),
                        ),
                    COPY(
                        "browser/chrome/browser/browser.dtd",
                        "fullscreenWarning.afterDomain.label",
                        trim=True
                        ),
                )
            )])

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from("""
fullscreen-warning-no-domain = { COPY(from_path, "fullscreenWarning.generic.label") }

fullscreen-exit-button = { COPY(from_path, "exitDOMFullscreen.button") }
fullscreen-exit-mac-button = { COPY(from_path, "exitDOMFullscreenMac.button") }
""", from_path="browser/chrome/browser/browser.dtd"))

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("pointerlock-warning-domain"),
                value=CONCAT_BEFORE_AFTER(
                    COPY(
                        "browser/chrome/browser/browser.dtd",
                        "pointerlockWarning.beforeDomain.label",
                        trim=True
                        ),
                    CONCAT(
                        FTL.TextElement('<span data-l10n-name="domain">'),
                        VARIABLE_REFERENCE('domain'),
                        FTL.TextElement('</span>'),
                        ),
                    COPY(
                        "browser/chrome/browser/browser.dtd",
                        "pointerlockWarning.afterDomain.label",
                        trim=True
                        ),
                )
            )]);

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        transforms_from("""
pointerlock-warning-no-domain = { COPY(from_path, "pointerlockWarning.generic.label") }
""", from_path="browser/chrome/browser/browser.dtd"))


