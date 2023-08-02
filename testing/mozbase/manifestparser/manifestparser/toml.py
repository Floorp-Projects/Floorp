# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import os

from tomlkit import parse
from tomlkit.exceptions import ParseError
from tomlkit.items import Array

from .ini import combine_fields

__all__ = ["read_toml"]


def read_toml(
    fp,
    defaults=None,
    default="DEFAULT",
    comments=None,
    separators=None,
    strict=True,
    handle_defaults=True,
):
    """
    read a .toml file and return a list of [(section, values)]
    - fp : file pointer or path to read
    - defaults : default set of variables
    - default : name of the section for the default section
    - comments : characters that if they start a line denote a comment
    - separators : strings that denote key, value separation in order
    - strict : whether to be strict about parsing
    - handle_defaults : whether to incorporate defaults into each section
    """

    # variables
    defaults = defaults or {}
    default_section = {}
    comments = comments or ("#",)
    separators = separators or ("=", ":")
    sections = []
    if isinstance(fp, str):
        filename = fp
        fp = io.open(fp, encoding="utf-8")
    elif hasattr(fp, "name"):
        filename = fp.name
    else:
        filename = "unknown"
    contents = fp.read()

    # Use tomlkit to parse the file contents
    try:
        manifest = parse(contents)
    except ParseError as pe:
        raise IOError(f"Error parsing TOML manifest file {filename}: {pe}")

    # handle each section of the manifest
    for section in manifest.keys():
        current_section = {}
        for key in manifest[section].keys():
            val = manifest[section][key]
            if isinstance(val, bool):
                if val:
                    val = "true"
                else:
                    val = "false"
            elif isinstance(val, Array):
                new_val = ""  # stay bug-for-bug compatible witn INI processing
                for v in val:
                    new_val += os.linesep + str(v)
                val = new_val
            else:
                val = str(val)  # coerce to str
                if " = " in val:
                    raise Exception(
                        f"Should not assign in {key} condition for {section}"
                    )
            current_section[key] = val
        if section.lower() == default.lower():
            default_section = current_section
            # DEFAULT does NOT appear in the output
        else:
            sections.append((section, current_section))

    # merge global defaults with the DEFAULT section
    defaults = combine_fields(defaults, default_section)
    if handle_defaults:
        # merge combined defaults into each section
        sections = [(i, combine_fields(defaults, j)) for i, j in sections]

    return sections, defaults
