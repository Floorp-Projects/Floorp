# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1733495 - Convert plugins.properties to Fluent, part {index}."""

    source = "dom/chrome/plugins.properties"
    target = "toolkit/toolkit/about/aboutPlugins.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("plugins-gmp-license-info"),
                value=COPY(source, "gmp_license_info"),
            ),
            FTL.Message(
                id=FTL.Identifier("plugins-gmp-privacy-info"),
                value=COPY(source, "gmp_privacy_info"),
            ),
            FTL.Message(
                id=FTL.Identifier("plugins-openh264-name"),
                value=COPY(source, "openH264_name"),
            ),
            FTL.Message(
                id=FTL.Identifier("plugins-openh264-description"),
                value=COPY(source, "openH264_description2"),
            ),
            FTL.Message(
                id=FTL.Identifier("plugins-widevine-name"),
                value=COPY(source, "widevine_description"),
            ),
            FTL.Message(
                id=FTL.Identifier("plugins-widevine-description"),
                value=COPY(source, "cdm_description2"),
            ),
        ],
    )
