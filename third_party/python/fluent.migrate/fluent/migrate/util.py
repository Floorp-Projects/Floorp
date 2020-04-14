# coding=utf8
from __future__ import unicode_literals
from __future__ import absolute_import

import textwrap

import fluent.syntax.ast as FTL
from fluent.syntax.parser import FluentParser, FluentParserStream


fluent_parser = FluentParser(with_spans=False)


def parse(Parser, string):
    if Parser is FluentParser:
        return fluent_parser.parse(string)

    # Parsing a legacy resource.

    # Parse the string into the internal Context.
    parser = Parser()
    # compare-locales expects ASCII strings.
    parser.readContents(string.encode('utf8'))
    # Transform the parsed result which is an iterator into a dict.
    return {ent.key: ent for ent in parser}


def ftl_resource_to_ast(code):
    return fluent_parser.parse(ftl(code))


def ftl_resource_to_json(code):
    return fluent_parser.parse(ftl(code)).to_json()


def ftl_pattern_to_json(code):
    ps = FluentParserStream(ftl(code))
    return fluent_parser.maybe_get_pattern(ps).to_json()


def to_json(merged_iter):
    return {
        path: resource.to_json()
        for path, resource in merged_iter
    }


LOCALIZABLE_ENTRIES = (FTL.Message, FTL.Term)


def get_message(body, ident):
    """Get message called `ident` from the `body` iterable."""
    for entity in body:
        if isinstance(entity, LOCALIZABLE_ENTRIES) and entity.id.name == ident:
            return entity


def get_transform(body, ident):
    """Get entity called `ident` from the `body` iterable."""
    for transform in body:
        if transform.id.name == ident:
            return transform


def ftl(code):
    """Nicer indentation for FTL code.

    The code returned by this function is meant to be compared against the
    output of the FTL Serializer.  The input code will end with a newline to
    match the output of the serializer.
    """

    # The code might be triple-quoted.
    code = code.lstrip('\n')

    return textwrap.dedent(code)


def fold(fun, node, init):
    """Reduce `node` to a single value using `fun`.

    Apply `fun` against an accumulator and each subnode of `node` (in postorder
    traversal) to reduce it to a single value.
    """

    def fold_(vals, acc):
        if not vals:
            return acc

        head = list(vals)[0]
        tail = list(vals)[1:]

        if isinstance(head, FTL.BaseNode):
            acc = fold(fun, head, acc)
        if isinstance(head, list):
            acc = fold_(head, acc)
        if isinstance(head, dict):
            acc = fold_(head.values(), acc)

        return fold_(tail, fun(acc, head))

    return fold_(vars(node).values(), init)
