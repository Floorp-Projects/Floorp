# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL

from fluent.migrate.transforms import TransformPattern


class INSERT_VAR_IN_TAG(TransformPattern):
    def visit_TextElement(self, node):
        old_str = node.value
        new_str = old_str
        # update-downloading / update-downloading-message
        new_str = new_str.replace(
            '<label data-l10n-name="download-status"/>',
            '<label data-l10n-name="download-status">{ $transfer }</label>',
        )
        # update-manual / update-internal-error
        new_str = new_str.replace(
            '<label data-l10n-name="manual-link"/>',
            '<label data-l10n-name="manual-link">{ $displayUrl }</label>',
        )
        # aboutdialog-update-manual
        new_str = new_str.replace(
            '<a data-l10n-name="manual-link"/>',
            '<a data-l10n-name="manual-link">{ $displayUrl }</a>',
        )
        if old_str == new_str and "$" not in old_str:
            print("Warning: Failed to insert var in link in {}".format(old_str))
        else:
            node.value = new_str
        return node


def migrate(ctx):
    """Bug 1835559, insert textContent var in a/label elements, part {index}."""

    aboutDialog_ftl = "browser/browser/aboutDialog.ftl"
    ctx.add_transforms(
        aboutDialog_ftl,
        aboutDialog_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("settings-update-downloading"),
                value=INSERT_VAR_IN_TAG(aboutDialog_ftl, "update-downloading"),
            ),
            FTL.Message(
                id=FTL.Identifier("aboutdialog-update-downloading"),
                value=INSERT_VAR_IN_TAG(aboutDialog_ftl, "update-downloading-message"),
            ),
            # Note: while we're renaming anyway: strip "aboutdialog-" prefix
            # because it is used in main.inc.xhtml, not aboutDialog.xhtml.
            FTL.Message(
                id=FTL.Identifier("settings-update-manual-with-link"),
                value=INSERT_VAR_IN_TAG(aboutDialog_ftl, "aboutdialog-update-manual"),
            ),
            # This is the actual update-manual message in aboutDialog.xhtml.
            FTL.Message(
                id=FTL.Identifier("aboutdialog-update-manual-with-link"),
                value=INSERT_VAR_IN_TAG(aboutDialog_ftl, "update-manual"),
            ),
            FTL.Message(
                id=FTL.Identifier("update-internal-error2"),
                value=INSERT_VAR_IN_TAG(aboutDialog_ftl, "update-internal-error"),
            ),
        ],
    )
