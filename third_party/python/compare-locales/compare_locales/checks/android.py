# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals

import re
from xml.dom import minidom

from .base import Checker
from ..parser.android import textContent


class AndroidChecker(Checker):
    pattern = re.compile('(.*)?strings.*\\.xml$')

    def check(self, refEnt, l10nEnt):
        '''Given the reference and localized Entities, performs checks.

        This is a generator yielding tuples of
        - "warning" or "error", depending on what should be reported,
        - tuple of line, column info for the error within the string
        - description string to be shown in the report
        '''
        refNode = refEnt.node
        l10nNode = l10nEnt.node
        # Apples and oranges, error out.
        if refNode.nodeName != l10nNode.nodeName:
            yield ("error", 0, "Incompatible resource types", "android")
            return
        # Once we start parsing more resource types, make sure to add checks
        # for them.
        if refNode.nodeName != "string":
            yield ("warning", 0, "Unsupported resource type", "android")
            return
        for report_tuple in self.check_string([refNode], l10nEnt):
            yield report_tuple

    def check_string(self, refs, l10nEnt):
        '''Check a single string literal against a list of references.

        There should be multiple nodes given for <plurals> or <string-array>.
        '''
        l10n = l10nEnt.node
        if self.not_translatable(l10n, *refs):
            yield (
                "error",
                0,
                "strings must be translatable",
                "android"
            )
            return
        if self.no_at_string(l10n):
            yield (
                "error",
                0,
                "strings must be translatable",
                "android"
            )
            return
        if self.no_at_string(*refs):
            yield (
                "warning",
                0,
                "strings must be translatable",
                "android"
            )
        if self.non_simple_data(l10n):
            yield (
                "error",
                0,
                "Only plain text allowed, "
                "or one CDATA surrounded by whitespace",
                "android"
            )
            return
        for report_tuple in check_apostrophes(l10nEnt.val):
            yield report_tuple

        params, errors = get_params(refs)
        for error, pos in errors:
            yield (
                "warning",
                pos,
                error,
                "android"
            )
        if params:
            for report_tuple in check_params(params, l10nEnt.val):
                yield report_tuple

    def not_translatable(self, *nodes):
        return any(
            node.hasAttribute("translatable")
            and node.getAttribute("translatable") == "false"
            for node in nodes
        )

    def no_at_string(self, *ref_nodes):
        '''Android allows to reference other strings by using
        @string/identifier
        instead of the actual value. Those references don't belong into
        a localizable file, warn on that.
        '''
        return any(
            textContent(node).startswith('@string/')
            for node in ref_nodes
        )

    def non_simple_data(self, node):
        '''Only allow single text nodes, or, a single CDATA node
        surrounded by whitespace.
        '''
        cdata = [
            child
            for child in node.childNodes
            if child.nodeType == minidom.Node.CDATA_SECTION_NODE
        ]
        if len(cdata) == 0:
            if node.childNodes.length == 0:
                # empty translation is OK
                return False
            if node.childNodes.length != 1:
                return True
            return node.childNodes[0].nodeType != minidom.Node.TEXT_NODE
        if len(cdata) > 1:
            return True
        for child in node.childNodes:
            if child == cdata[0]:
                continue
            if child.nodeType != minidom.Node.TEXT_NODE:
                return True
            if child.data.strip() != "":
                return True
        return False


silencer = re.compile(r'\\.|""')


def check_apostrophes(string):
    '''Check Android logic for quotes and apostrophes.

    If you have an apostrophe (') in your string, you must either escape it
    with a backslash (\') or enclose the string in double-quotes (").

    Unescaped quotes are not visually shown on Android, but they're
    also harmless, so we're not checking for quotes. We might do once we're
    better at checking for inline XML, which is full of quotes.
    Pairing quotes as in '""' is bad, though, so report errors for that.
    Mostly, because it's hard to tell if a string is consider quoted or not
    by Android in the end.

    https://developer.android.com/guide/topics/resources/string-resource#escaping_quotes
    '''
    for m in re.finditer('""', string):
        yield (
            "error",
            m.start(),
            "Double straight quotes not allowed",
            "android"
        )
    string = silencer.sub("  ", string)

    is_quoted = string.startswith('"') and string.endswith('"')
    if not is_quoted:
        # apostrophes need to be escaped
        for m in re.finditer("'", string):
            yield (
                "error",
                m.start(),
                "Apostrophe must be escaped",
                "android"
            )


def get_params(refs):
    '''Get printf parameters and internal errors.

    Returns a sparse map of positions to formatter, and a list
    of errors. Errors covered so far are mismatching formatters.
    '''
    params = {}
    errors = []
    next_implicit = 1
    for ref in refs:
        if isinstance(ref, minidom.Node):
            ref = textContent(ref)
        for m in re.finditer(r'%(?P<order>[1-9]\$)?(?P<format>[sSd])', ref):
            order = m.group('order')
            if order:
                order = int(order[0])
            else:
                order = next_implicit
                next_implicit += 1
            fmt = m.group('format')
            if order not in params:
                params[order] = fmt
            else:
                # check for consistency errors
                if params[order] == fmt:
                    continue
                msg = "Conflicting formatting, %{order}${f1} vs %{order}${f2}"
                errors.append((
                    msg.format(order=order, f1=fmt, f2=params[order]),
                    m.start()
                ))
    return params, errors


def check_params(params, string):
    '''Compare the printf parameters in the given string to the reference
    parameters.

    Also yields errors that are internal to the parameters inside string,
    as found by `get_params`.
    '''
    lparams, errors = get_params([string])
    for error, pos in errors:
        yield (
            "error",
            pos,
            error,
            "android"
        )
    # Compare reference for each localized parameter.
    # If there's no reference found, error, as an out-of-bounds
    # parameter crashes.
    # This assumes that all parameters are actually used in the reference,
    # which should be OK.
    # If there's a mismatch in the formatter, error.
    for order in sorted(lparams):
        if order not in params:
            yield (
                "error",
                0,
                "Formatter %{}${} not found in reference".format(
                    order, lparams[order]
                ),
                "android"
            )
        elif params[order] != lparams[order]:
            yield (
                "error",
                0,
                "Mismatching formatter",
                "android"
            )
