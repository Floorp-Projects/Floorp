# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
from compare_locales import mozpath


# The local path where we write the config files to
TARGET_PATH = b"_configs"


def process_config(toml_content):
    """Process TOML configuration content to match the l10n setup for
    the reference localization, return target_path and content.

    The code adjusts basepath, [[paths]], and [[includes]]
    """
    # adjust basepath in content. '.' works in practice, also in theory?
    new_base = mozpath.relpath(b".", TARGET_PATH)
    if not new_base:
        new_base = b"."  # relpath to '.' is '', sadly
    base_line = b'\nbasepath = "%s"' % new_base
    content1 = re.sub(br"^\s*basepath\s*=\s*.+", base_line, toml_content, flags=re.M)

    # process [[paths]]
    start = 0
    content2 = b""
    for m in re.finditer(
        br"\[\[\s*paths\s*\]\].+?(?=\[|\Z)", content1, re.M | re.DOTALL
    ):
        content2 += content1[start : m.start()]
        path_content = m.group()
        l10n_line = re.search(br"^\s*l10n\s*=.*$", path_content, flags=re.M).group()
        # remove variable expansions
        new_reference = re.sub(br"{\s*\S+\s*}", b"", l10n_line)
        # make the l10n a reference line
        new_reference = re.sub(br"^(\s*)l10n(\s*=)", br"\1reference\2", new_reference)
        content2 += re.sub(
            br"^\s*reference\s*=.*$", new_reference, path_content, flags=re.M
        )
        start = m.end()
    content2 += content1[start:]

    start = 0
    content3 = b""
    for m in re.finditer(
        br"\[\[\s*includes\s*\]\].+?(?=\[|\Z)", content2, re.M | re.DOTALL
    ):
        content3 += content2[start : m.start()]
        include_content = m.group()
        m_ = re.search(br'^\s*path = "(.+?)"', include_content, flags=re.M)
        content3 += (
            include_content[: m_.start(1)]
            + generate_filename(m_.group(1))
            + include_content[m_.end(1) :]
        )
        start = m.end()
    content3 += content2[start:]

    return content3


def generate_filename(path):
    segs = path.split(b"/")
    # strip /locales/ from filename
    segs = [seg for seg in segs if seg != b"locales"]
    # strip variables from filename
    segs = [seg for seg in segs if not seg.startswith(b"{") and not seg.endswith(b"}")]
    if segs[-1] == b"l10n.toml":
        segs.pop()
        segs[-1] += b".toml"
    outpath = b"-".join(segs)
    if TARGET_PATH != b".":
        # prepend the target path, if it's not '.'
        outpath = mozpath.join(TARGET_PATH, outpath)
    return outpath
