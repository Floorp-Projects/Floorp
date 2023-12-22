# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import os
import re

from .ini import combine_fields

__all__ = ["read_toml", "alphabetize_toml_str", "add_skip_if"]

FILENAME_REGEX = r"^([A-Za-z0-9_./-]*)([Bb][Uu][Gg])([-_]*)([0-9]+)([A-Za-z0-9_./-]*)$"


def sort_paths_keyfn(k):
    sort_paths_keyfn.rx = getattr(sort_paths_keyfn, "rx", None)  # static
    if sort_paths_keyfn.rx is None:
        sort_paths_keyfn.rx = re.compile(FILENAME_REGEX)
    name = str(k)
    if name == "DEFAULT":
        return ""
    m = sort_paths_keyfn.rx.findall(name)
    if len(m) == 1 and len(m[0]) == 5:
        prefix = m[0][0]  # text before "Bug"
        bug = m[0][1]  # the word "Bug"
        underbar = m[0][2]  # underbar or dash (optional)
        num = m[0][3]  # the bug id
        suffix = m[0][4]  # text after the bug id
        name = f"{prefix}{bug.lower()}{underbar}{int(num):09d}{suffix}"
        return name
    return name


def sort_paths(paths):
    """
    Returns a list of paths (tests) in a manifest in alphabetical order.
    Ensures DEFAULT is first and filenames with a bug number are
    in the proper order.
    """
    return sorted(paths, key=sort_paths_keyfn)


def parse_toml_str(contents):
    """
    Parse TOML contents using toml
    """
    import toml

    error = None
    manifest = None
    try:
        manifest = toml.loads(contents)
    except toml.TomlDecodeError as pe:
        error = str(pe)
    return error, manifest


def parse_tomlkit_str(contents):
    """
    Parse TOML contents using tomlkit
    """
    import tomlkit
    from tomlkit.exceptions import TOMLKitError

    error = None
    manifest = None
    try:
        manifest = tomlkit.parse(contents)
    except TOMLKitError as pe:
        error = str(pe)
    return error, manifest


def read_toml(
    fp,
    defaults=None,
    default="DEFAULT",
    _comments=None,
    _separators=None,
    strict=True,
    handle_defaults=True,
    document=False,
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
    - document: read TOML with tomlkit and return source in test["document"]
    """

    # variables
    defaults = defaults or {}
    default_section = {}
    sections = []
    if isinstance(fp, str):
        filename = fp
        fp = io.open(fp, encoding="utf-8")
    elif hasattr(fp, "name"):
        filename = fp.name
    else:
        filename = "unknown"
    contents = fp.read()
    inline_comment_rx = re.compile(r"\s#.*$")

    if document:  # Use tomlkit to parse the file contents
        error, manifest = parse_tomlkit_str(contents)
    else:
        error, manifest = parse_toml_str(contents)
    if error:
        raise IOError(f"Error parsing TOML manifest file {filename}: {error}")

    # handle each section of the manifest
    for section in manifest.keys():
        current_section = {}
        for key in manifest[section].keys():
            val = manifest[section][key]
            if isinstance(val, bool):  # must coerce to lowercase string
                if val:
                    val = "true"
                else:
                    val = "false"
            elif isinstance(val, list):
                new_vals = ""
                for v in val:
                    if len(new_vals) > 0:
                        new_vals += os.linesep
                    new_val = str(v).strip()  # coerce to str
                    comment_found = inline_comment_rx.search(new_val)
                    if comment_found:
                        new_val = new_val[0 : comment_found.span()[0]]
                    if " = " in new_val:
                        raise Exception(
                            f"Should not assign in {key} condition for {section}"
                        )
                    new_vals += new_val
                val = new_vals
            else:
                val = str(val).strip()  # coerce to str
                comment_found = inline_comment_rx.search(val)
                if comment_found:
                    val = val[0 : comment_found.span()[0]]
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

    if not document:
        manifest = None
    return sections, defaults, manifest


def alphabetize_toml_str(manifest):
    """
    Will take a TOMLkit manifest document (i.e. from a previous invocation
    of read_toml(..., document=True) and accessing the document
    from mp.source_documents[filename]) and return it as a string
    in sorted order by section (i.e. test file name, taking bug ids into consideration).
    """

    from tomlkit import document, dumps, table
    from tomlkit.items import KeyType, SingleKey

    preamble = ""
    for k, v in manifest.body:
        if k is None:
            preamble += v.as_string()
        else:
            break
    new_manifest = document()
    if "DEFAULT" in manifest:
        new_manifest.add("DEFAULT", manifest["DEFAULT"])
    else:
        new_manifest.add("DEFAULT", table())
    sections = sort_paths([k for k in manifest.keys() if k != "DEFAULT"])
    for k in sections:
        if k.find('"') >= 0:
            section = k
        else:
            section = SingleKey(k, KeyType.Basic)
        new_manifest.add(section, manifest[k])
    manifest_str = dumps(new_manifest)
    # tomlkit fixups
    manifest_str = preamble + manifest_str.replace('"",]', "]")
    while manifest_str.endswith("\n\n"):
        manifest_str = manifest_str[:-1]
    return manifest_str


def _simplify_comment(comment):
    """Remove any leading #, but preserve leading whitespace in comment"""

    length = len(comment)
    i = 0
    j = -1  # remove exactly one space
    while i < length and comment[i] in " #":
        i += 1
        if comment[i] == " ":
            j += 1
    comment = comment[i:]
    if j > 0:
        comment = " " * j + comment
    return comment.rstrip()


def add_skip_if(manifest, filename, condition, bug=None):
    """
    Will take a TOMLkit manifest document (i.e. from a previous invocation
    of read_toml(..., document=True) and accessing the document
    from mp.source_documents[filename]) and return it as a string
    in sorted order by section (i.e. test file name, taking bug ids into consideration).
    """
    from tomlkit import array
    from tomlkit.items import Comment, String, Whitespace

    if filename not in manifest:
        raise Exception(f"TOML manifest does not contain section: {filename}")
    keyvals = manifest[filename]
    first = None
    first_comment = ""
    skip_if = None
    existing = False  # this condition is already present
    if "skip-if" in keyvals:
        skip_if = keyvals["skip-if"]
        if len(skip_if) == 1:
            for e in skip_if._iter_items():
                if not first:
                    if not isinstance(e, Whitespace):
                        first = e.as_string().strip('"')
                else:
                    c = e.as_string()
                    if c != ",":
                        first_comment += c
            if skip_if.trivia is not None:
                first_comment += skip_if.trivia.comment
    mp_array = array()
    if skip_if is None:  # add the first one line entry to the table
        mp_array.add_line(condition, indent="", add_comma=False, newline=False)
        if bug is not None:
            mp_array.comment(bug)
        skip_if = {"skip-if": mp_array}
        keyvals.update(skip_if)
    else:
        if first is not None:
            if first == condition:
                existing = True
            if first_comment is not None:
                mp_array.add_line(
                    first, indent="  ", comment=_simplify_comment(first_comment)
                )
            else:
                mp_array.add_line(first, indent="  ")
        if len(skip_if) > 1:
            e_condition = None
            e_comment = None
            for e in skip_if._iter_items():
                if isinstance(e, String):
                    if e_condition is not None:
                        if e_comment is not None:
                            mp_array.add_line(
                                e_condition, indent="  ", comment=e_comment
                            )
                            e_comment = None
                        else:
                            mp_array.add_line(e_condition, indent="  ")
                        e_condition = None
                    if len(e) > 0:
                        e_condition = e.as_string().strip('"')
                        if e_condition == condition:
                            existing = True
                elif isinstance(e, Comment):
                    e_comment = _simplify_comment(e.as_string())
            if e_condition is not None:
                if e_comment is not None:
                    mp_array.add_line(e_condition, indent="  ", comment=e_comment)
                else:
                    mp_array.add_line(e_condition, indent="  ")
        if not existing:
            if bug is not None:
                mp_array.add_line(condition, indent="  ", comment=bug)
            else:
                mp_array.add_line(condition, indent="  ")
        mp_array.add_line("", indent="")  # fixed in write_toml_str
        skip_if = {"skip-if": mp_array}
        del keyvals["skip-if"]
        keyvals.update(skip_if)
