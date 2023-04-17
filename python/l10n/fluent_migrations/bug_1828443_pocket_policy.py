# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL

from fluent.migrate.transforms import TransformPattern


class REPLACE_POCKET(TransformPattern):
    # Replace Pocket with term `{ -pocket-brand-name }`
    def visit_TextElement(self, node):
        node.value = node.value.replace("Pocket", "{ -pocket-brand-name }")

        return node


def migrate(ctx):
    """Bug 1828443 - Use Fluent term for Pocket in policy string, part {index}."""

    ftl_file = "browser/browser/policies/policies-descriptions.ftl"
    ctx.add_transforms(
        ftl_file,
        ftl_file,
        [
            FTL.Message(
                id=FTL.Identifier("policy-DisablePocket2"),
                value=REPLACE_POCKET(ftl_file, "policy-DisablePocket"),
            ),
        ],
    )
