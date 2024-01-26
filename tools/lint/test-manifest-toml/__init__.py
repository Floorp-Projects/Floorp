# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import os
import re

from manifestparser import TestManifest
from manifestparser.toml import DEFAULT_SECTION, alphabetize_toml_str, sort_paths
from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozpack import path as mozpath
from tomlkit.items import Array, Table

SECTION_REGEX = r"^\[.*\]$"
DISABLE_REGEX = r"^[ \t]*#[ \t]*\[.*\]"


def make_result(path, message, is_error=False):
    if is_error:
        level = "error"
    else:
        level = "warning"
    result = {
        "path": path,
        "lineno": 0,  # tomlkit does not report lineno/column
        "column": 0,
        "message": message,
        "level": level,
    }
    return result


def lint(paths, config, fix=None, **lintargs):
    results = []
    fixed = 0
    topsrcdir = lintargs["root"]
    file_names = list(expand_exclusions(paths, config, topsrcdir))
    file_names = [os.path.normpath(f) for f in file_names]
    section_rx = re.compile(SECTION_REGEX, flags=re.M)
    disable_rx = re.compile(DISABLE_REGEX, flags=re.M)

    for file_name in file_names:
        path = mozpath.relpath(file_name, topsrcdir)
        os.path.basename(file_name)
        parser = TestManifest(use_toml=True, document=True)

        try:
            parser.read(file_name)
        except Exception:
            r = make_result(path, "The manifest is not valid TOML.", True)
            results.append(result.from_config(config, **r))
            continue

        manifest = parser.source_documents[file_name]
        manifest_str = io.open(file_name, "r", encoding="utf-8").read()

        if not DEFAULT_SECTION in manifest:
            r = make_result(
                path, f"The manifest does not start with a [{DEFAULT_SECTION}] section."
            )
            if fix:
                fixed += 1
            results.append(result.from_config(config, **r))

        sections = [k for k in manifest.keys() if k != DEFAULT_SECTION]
        sorted_sections = sort_paths(sections)
        if sections != sorted_sections:
            r = make_result(
                path, "The manifest sections are not in alphabetical order."
            )
            if fix:
                fixed += 1
            results.append(result.from_config(config, **r))

        m = section_rx.findall(manifest_str)
        if len(m) > 0:
            for section_match in m:
                section = section_match[1:-1]
                if section == DEFAULT_SECTION:
                    continue
                if not section.startswith('"'):
                    r = make_result(
                        path, f"The section name must be double quoted: [{section}]"
                    )
                    if fix:
                        fixed += 1
                    results.append(result.from_config(config, **r))

        m = disable_rx.findall(manifest_str)
        if len(m) > 0:
            for disabled_section in m:
                r = make_result(
                    path,
                    f"Use 'disabled = \"<reason>\"' to disable a test instead of a comment: {disabled_section}",
                    True,
                )
                results.append(result.from_config(config, **r))

        for section, keyvals in manifest.body:
            if section is None:
                continue
            if not isinstance(keyvals, Table):
                r = make_result(
                    path, f"Bad assignment in preamble: {section} = {keyvals}", True
                )
                results.append(result.from_config(config, **r))
            else:
                for k, v in keyvals.items():
                    if k.endswith("-if"):
                        if not isinstance(v, Array):
                            r = make_result(
                                path,
                                f'Value for conditional must be an array: {k} = "{v}"',
                                True,
                            )
                            results.append(result.from_config(config, **r))
                        else:
                            for e in v:
                                if e.find("||") > 0 and e.find("&&") < 0:
                                    r = make_result(
                                        path,
                                        f'Value for conditional must not include explicit ||, instead put on multiple lines: {k} = [ ... "{e}" ... ]',
                                        True,
                                    )
                                    results.append(result.from_config(config, **r))

        if fix:
            manifest_str = alphabetize_toml_str(manifest)
            fp = io.open(file_name, "w", encoding="utf-8", newline="\n")
            fp.write(manifest_str)
            fp.close()

    return {"results": results, "fixed": fixed}
