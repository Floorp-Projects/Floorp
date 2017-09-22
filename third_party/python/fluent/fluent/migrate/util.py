# coding=utf8
from __future__ import unicode_literals

import fluent.syntax.ast as FTL
from fluent.syntax.parser import FluentParser
from fluent.util import ftl


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


def ftl_message_to_json(code):
    return fluent_parser.parse_entry(ftl(code)).to_json()


def to_json(merged_iter):
    return {
        path: resource.to_json()
        for path, resource in merged_iter
    }


def get_message(body, ident):
    """Get message called `ident` from the `body` iterable."""
    for entity in body:
        if isinstance(entity, FTL.Message) and entity.id.name == ident:
            return entity


def get_transform(body, ident):
    """Get entity called `ident` from the `body` iterable."""
    for transform in body:
        if transform.id.name == ident:
            return transform
