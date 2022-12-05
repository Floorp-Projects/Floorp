# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY, LegacySource, Transform


class CONTRIB(LegacySource):
    """Custom transform converting a list of contributors from the old RDF based
    list of entries into a comma delimited list.

    Based on code from python/mozbuild/mozbuild/action/langpack_manifest.py
    """

    def __call__(self, ctx):
        element: FTL.TextElement = super(CONTRIB, self).__call__(ctx)
        str: str = element.value.replace("<em:contributor>", "")
        tokens = str.split("</em:contributor>")
        tokens = map(lambda t: t.strip(), tokens)
        tokens = filter(lambda t: t != "", tokens)
        element.value = ", ".join(tokens)
        return Transform.pattern_of(element)


def migrate(ctx):
    """Bug 1802128 - Convert langpack defines.inc to Fluent, part {index}."""

    browser = "browser/defines.inc"
    target = "browser/langpack-metadata.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("langpack-creator"),
                value=COPY(browser, "MOZ_LANGPACK_CREATOR"),
            ),
            FTL.Message(
                id=FTL.Identifier("langpack-contributors"),
                value=CONTRIB(browser, "MOZ_LANGPACK_CONTRIBUTORS"),
            ),
        ],
    )
