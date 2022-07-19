# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import importlib

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx.util.docstrings import prepare_docstring
from sphinx.util.docutils import ReferenceRole


def function_reference(f, attr, args, doc):
    lines = []

    lines.extend(
        [
            f,
            "-" * len(f),
            "",
        ]
    )

    docstring = prepare_docstring(doc)

    lines.extend(
        [
            docstring[0],
            "",
        ]
    )

    arg_types = []

    for t in args:
        if isinstance(t, list):
            inner_types = [t2.__name__ for t2 in t]
            arg_types.append(" | ".join(inner_types))
            continue

        arg_types.append(t.__name__)

    arg_s = "(%s)" % ", ".join(arg_types)

    lines.extend(
        [
            ":Arguments: %s" % arg_s,
            "",
        ]
    )

    lines.extend(docstring[1:])
    lines.append("")

    return lines


def variable_reference(v, st_type, in_type, doc):
    lines = [
        v,
        "-" * len(v),
        "",
    ]

    docstring = prepare_docstring(doc)

    lines.extend(
        [
            docstring[0],
            "",
        ]
    )

    lines.extend(
        [
            ":Storage Type: ``%s``" % st_type.__name__,
            ":Input Type: ``%s``" % in_type.__name__,
            "",
        ]
    )

    lines.extend(docstring[1:])
    lines.append("")

    return lines


def special_reference(v, func, typ, doc):
    lines = [
        v,
        "-" * len(v),
        "",
    ]

    docstring = prepare_docstring(doc)

    lines.extend(
        [
            docstring[0],
            "",
            ":Type: ``%s``" % typ.__name__,
            "",
        ]
    )

    lines.extend(docstring[1:])
    lines.append("")

    return lines


def format_module(m):
    lines = []

    lines.extend(
        [
            ".. note::",
            "   moz.build files' implementation includes a ``Path`` class.",
        ]
    )
    path_docstring_minus_summary = prepare_docstring(m.Path.__doc__)[2:]
    lines.extend(["   " + line for line in path_docstring_minus_summary])

    for subcontext, cls in sorted(m.SUBCONTEXTS.items()):
        lines.extend(
            [
                ".. _mozbuild_subcontext_%s:" % subcontext,
                "",
                "Sub-Context: %s" % subcontext,
                "=============" + "=" * len(subcontext),
                "",
            ]
        )
        lines.extend(prepare_docstring(cls.__doc__))
        if lines[-1]:
            lines.append("")

        for k, v in sorted(cls.VARIABLES.items()):
            lines.extend(variable_reference(k, *v))

    lines.extend(
        [
            "Variables",
            "=========",
            "",
        ]
    )

    for v in sorted(m.VARIABLES):
        lines.extend(variable_reference(v, *m.VARIABLES[v]))

    lines.extend(
        [
            "Functions",
            "=========",
            "",
        ]
    )

    for func in sorted(m.FUNCTIONS):
        lines.extend(function_reference(func, *m.FUNCTIONS[func]))

    lines.extend(
        [
            "Special Variables",
            "=================",
            "",
        ]
    )

    for v in sorted(m.SPECIAL_VARIABLES):
        lines.extend(special_reference(v, *m.SPECIAL_VARIABLES[v]))

    return lines


class MozbuildSymbols(Directive):
    """Directive to insert mozbuild sandbox symbol information."""

    required_arguments = 1

    def run(self):
        module = importlib.import_module(self.arguments[0])
        fname = module.__file__
        if fname.endswith(".pyc"):
            fname = fname[0:-1]

        self.state.document.settings.record_dependencies.add(fname)

        # We simply format out the documentation as rst then feed it back
        # into the parser for conversion. We don't even emit ourselves, so
        # there's no record of us.
        self.state_machine.insert_input(format_module(module), fname)

        return []


class Searchfox(ReferenceRole):
    """Role which links a relative path from the source to it's searchfox URL.

    Can be used like:

        See :searchfox:`browser/base/content/browser-places.js` for more details.

    Will generate a link to
    ``https://searchfox.org/mozilla-central/source/browser/base/content/browser-places.js``

    The example above will use the path as the text, to use custom text:

        See :searchfox:`this file <browser/base/content/browser-places.js>` for
        more details.

    To specify a different source tree:

        See :searchfox:`mozilla-beta:browser/base/content/browser-places.js`
        for more details.
    """

    def run(self):
        base = "https://searchfox.org/{source}/source/{path}"

        if ":" in self.target:
            source, path = self.target.split(":", 1)
        else:
            source = "mozilla-central"
            path = self.target

        url = base.format(source=source, path=path)

        if self.has_explicit_title:
            title = self.title
        else:
            title = path

        node = nodes.reference(self.rawtext, title, refuri=url, **self.options)
        return [node], []


def setup(app):
    from moztreedocs import manager

    app.add_directive("mozbuildsymbols", MozbuildSymbols)
    app.add_role("searchfox", Searchfox())

    # Unlike typical Sphinx installs, our documentation is assembled from
    # many sources and staged in a common location. This arguably isn't a best
    # practice, but it was the easiest to implement at the time.
    #
    # Here, we invoke our custom code for staging/generating all our
    # documentation.
    manager.generate_docs(app)
    app.srcdir = manager.staging_dir
