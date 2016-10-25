# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

__all__ = ['read_ini', 'combine_fields']


def read_ini(fp, variables=None, default='DEFAULT', defaults_only=False,
             comments=';#', separators=('=', ':'), strict=True,
             handle_defaults=True):
    """
    read an .ini file and return a list of [(section, values)]
    - fp : file pointer or path to read
    - variables : default set of variables
    - default : name of the section for the default section
    - defaults_only : if True, return the default section only
    - comments : characters that if they start a line denote a comment
    - separators : strings that denote key, value separation in order
    - strict : whether to be strict about parsing
    - handle_defaults : whether to incorporate defaults into each section
    """

    # variables
    variables = variables or {}
    sections = []
    key = value = None
    section_names = set()
    if isinstance(fp, basestring):
        fp = file(fp)

    # read the lines
    for (linenum, line) in enumerate(fp.read().splitlines(), start=1):

        stripped = line.strip()

        # ignore blank lines
        if not stripped:
            # reset key and value to avoid continuation lines
            key = value = None
            continue

        # ignore comment lines
        if stripped[0] in comments:
            continue

        # check for a new section
        if len(stripped) > 2 and stripped[0] == '[' and stripped[-1] == ']':
            section = stripped[1:-1].strip()
            key = value = None

            # deal with DEFAULT section
            if section.lower() == default.lower():
                if strict:
                    assert default not in section_names
                section_names.add(default)
                current_section = variables
                continue

            if strict:
                # make sure this section doesn't already exist
                assert section not in section_names, "Section '%s' already found in '%s'" % (
                    section, section_names)

            section_names.add(section)
            current_section = {}
            sections.append((section, current_section))
            continue

        # if there aren't any sections yet, something bad happen
        if not section_names:
            raise Exception('No sections found')

        # (key, value) pair
        for separator in separators:
            if separator in stripped:
                key, value = stripped.split(separator, 1)
                key = key.strip()
                value = value.strip()

                if strict:
                    # make sure this key isn't already in the section or empty
                    assert key
                    if current_section is not variables:
                        assert key not in current_section

                current_section[key] = value
                break
        else:
            # continuation line ?
            if line[0].isspace() and key:
                value = '%s%s%s' % (value, os.linesep, stripped)
                current_section[key] = value
            else:
                # something bad happened!
                if hasattr(fp, 'name'):
                    filename = fp.name
                else:
                    filename = 'unknown'
                raise Exception("Error parsing manifest file '%s', line %s" %
                                (filename, linenum))

    # server-root is a special os path declared relative to the manifest file.
    # inheritance demands we expand it as absolute
    if 'server-root' in variables:
        root = os.path.join(os.path.dirname(fp.name),
                            variables['server-root'])
        variables['server-root'] = os.path.abspath(root)

    # return the default section only if requested
    if defaults_only:
        return [(default, variables)]

    global_vars = variables if handle_defaults else {}
    sections = [(i, combine_fields(global_vars, j)) for i, j in sections]
    return sections


def combine_fields(global_vars, local_vars):
    """
    Combine the given manifest entries according to the semantics of specific fields.
    This is used to combine manifest level defaults with a per-test definition.
    """
    if not global_vars:
        return local_vars
    if not local_vars:
        return global_vars
    field_patterns = {
        'skip-if': '(%s) || (%s)',
        'support-files': '%s %s',
    }
    final_mapping = global_vars.copy()
    for field_name, value in local_vars.items():
        if field_name not in field_patterns or field_name not in global_vars:
            final_mapping[field_name] = value
            continue
        global_value = global_vars[field_name]
        pattern = field_patterns[field_name]
        final_mapping[field_name] = pattern % (
            global_value.split('#')[0], value.split('#')[0])
    return final_mapping
