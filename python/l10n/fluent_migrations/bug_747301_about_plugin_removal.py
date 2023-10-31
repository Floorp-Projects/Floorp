# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 747301 - remove about:plugins, part {index}."""
    plugins_ftl = "toolkit/toolkit/about/aboutPlugins.ftl"
    addon_ftl = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        addon_ftl,
        addon_ftl,
        transforms_from(
            """
plugins-gmp-license-info = {COPY_PATTERN(from_path, "plugins-gmp-license-info")}
plugins-gmp-privacy-info = {COPY_PATTERN(from_path, "plugins-gmp-privacy-info")}

plugins-openh264-name = {COPY_PATTERN(from_path, "plugins-openh264-name")}
plugins-openh264-description = {COPY_PATTERN(from_path, "plugins-openh264-description")}

plugins-widevine-name = {COPY_PATTERN(from_path, "plugins-widevine-name")}
plugins-widevine-description = {COPY_PATTERN(from_path, "plugins-widevine-description")}
""",
            from_path=plugins_ftl,
        ),
    )
