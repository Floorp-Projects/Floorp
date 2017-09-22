# coding=utf8
"""Migration Transforms.

Transforms are AST nodes which describe how legacy translations should be
migrated.  They are created inert and only return the migrated AST nodes when
they are evaluated by a MergeContext.

All Transforms evaluate to Fluent Patterns. This makes them suitable for
defining migrations of values of message, attributes and variants.  The special
CONCAT Transform is capable of joining multiple Patterns returned by evaluating
other Transforms into a single Pattern.  It can also concatenate Fluent
Expressions, like MessageReferences and ExternalArguments.

The COPY, REPLACE and PLURALS Transforms inherit from Source which is a special
AST Node defining the location (the file path and the id) of the legacy
translation.  During the migration, the current MergeContext scans the
migration spec for Source nodes and extracts the information about all legacy
translations being migrated. Thus,

    COPY('file.dtd', 'hello')

is equivalent to:

    LITERAL(Source('file.dtd', 'hello'))

where LITERAL is a helper defined in the helpers.py module for creating Fluent
Patterns from the text passed as the argument.

The LITERAL helper and the special REPLACE_IN_TEXT Transforms are useful for
working with text rather than (path, key) source definitions.  This is the case
when the migrated translation requires some hardcoded text, e.g. <a> and </a>
when multiple translations become a single one with a DOM overlay.

    FTL.Message(
        id=FTL.Identifier('update-failed'),
        value=CONCAT(
            COPY('aboutDialog.dtd', 'update.failed.start'),
            LITERAL('<a>'),
            COPY('aboutDialog.dtd', 'update.failed.linkText'),
            LITERAL('</a>'),
            COPY('aboutDialog.dtd', 'update.failed.end'),
        )
    )

The REPLACE_IN_TEXT Transform also takes text as input, making in possible to
pass it as the foreach function of the PLURALS Transform.  In this case, each
slice of the plural string will be run through a REPLACE_IN_TEXT operation.
Those slices are strings, so a REPLACE(path, key, …) isn't suitable for them.

    FTL.Message(
        FTL.Identifier('delete-all'),
        value=PLURALS(
            'aboutDownloads.dtd',
            'deleteAll',
            EXTERNAL_ARGUMENT('num'),
            lambda text: REPLACE_IN_TEXT(
                text,
                {
                    '#1': EXTERNAL_ARGUMENT('num')
                }
            )
        )
    )
"""

from __future__ import unicode_literals

import fluent.syntax.ast as FTL
from .helpers import LITERAL


def evaluate(ctx, node):
    def eval_node(subnode):
        if isinstance(subnode, Transform):
            return subnode(ctx)
        else:
            return subnode

    return node.traverse(eval_node)


class Transform(FTL.BaseNode):
    def __call__(self, ctx):
        raise NotImplementedError


class Source(Transform):
    """Declare the source translation to be migrated with other transforms.

    When evaluated `Source` returns a simple string value.  All \\uXXXX from
    the original translations are converted beforehand to the literal
    characters they encode.

    HTML entities are left unchanged for now because we can't know if they
    should be converted to the characters they represent or not.  Consider the
    following example in which `&amp;` could be replaced with the literal `&`:

        Privacy &amp; History

    vs. these two examples where the HTML encoding should be preserved:

        Erreur&nbsp;!
        Use /help &lt;command&gt; for more information.

    """

    # XXX Perhaps there's a strict subset of HTML entities which must or must
    # not be replaced?

    def __init__(self, path, key):
        self.path = path
        self.key = key

    def __call__(self, ctx):
        return ctx.get_source(self.path, self.key)


class COPY(Source):
    """Create a Pattern with the translation value from the given source."""

    def __call__(self, ctx):
        source = super(self.__class__, self).__call__(ctx)
        return LITERAL(source)


class REPLACE_IN_TEXT(Transform):
    """Replace various placeables in the translation with FTL placeables.

    The original placeables are defined as keys on the `replacements` dict.
    For each key the value is defined as a list of FTL Expressions to be
    interpolated.
    """

    def __init__(self, value, replacements):
        self.value = value
        self.replacements = replacements

    def __call__(self, ctx):

        # Only replace placeable which are present in the translation.
        replacements = {
            key: evaluate(ctx, repl)
            for key, repl in self.replacements.iteritems()
            if key in self.value
        }

        # Order the original placeables by their position in the translation.
        keys_in_order = sorted(
            replacements.keys(),
            lambda x, y: self.value.find(x) - self.value.find(y)
        )

        # Used to reduce the `keys_in_order` list.
        def replace(acc, cur):
            """Convert original placeables and text into FTL Nodes.

            For each original placeable the translation will be partitioned
            around it and the text before it will be converted into an
            `FTL.TextElement` and the placeable will be replaced with its
            replacement. The text following the placebale will be fed again to
            the `replace` function.
            """

            parts, rest = acc
            before, key, after = rest.value.partition(cur)

            placeable = FTL.Placeable(replacements[key])

            # Return the elements found and converted so far, and the remaining
            # text which hasn't been scanned for placeables yet.
            return (
                parts + [FTL.TextElement(before), placeable],
                FTL.TextElement(after)
            )

        def is_non_empty(elem):
            """Used for filtering empty `FTL.TextElement` nodes out."""
            return not isinstance(elem, FTL.TextElement) or len(elem.value)

        # Start with an empty list of elements and the original translation.
        init = ([], FTL.TextElement(self.value))
        parts, tail = reduce(replace, keys_in_order, init)

        # Explicitly concat the trailing part to get the full list of elements
        # and filter out the empty ones.
        elements = filter(is_non_empty, parts + [tail])

        return FTL.Pattern(elements)


class REPLACE(Source):
    """Create a Pattern with interpolations from given source.

    Interpolations in the translation value from the given source will be
    replaced with FTL placeables using the `REPLACE_IN_TEXT` transform.
    """

    def __init__(self, path, key, replacements):
        super(self.__class__, self).__init__(path, key)
        self.replacements = replacements

    def __call__(self, ctx):
        value = super(self.__class__, self).__call__(ctx)
        return REPLACE_IN_TEXT(value, self.replacements)(ctx)


class PLURALS(Source):
    """Create a Pattern with plurals from given source.

    Build an `FTL.SelectExpression` with the supplied `selector` and variants
    extracted from the source.  The source needs to be a semicolon-separated
    list of variants.  Each variant will be run through the `foreach` function,
    which should return an `FTL.Node` or a `Transform`.
    """

    def __init__(self, path, key, selector, foreach=LITERAL):
        super(self.__class__, self).__init__(path, key)
        self.selector = selector
        self.foreach = foreach

    def __call__(self, ctx):
        value = super(self.__class__, self).__call__(ctx)
        selector = evaluate(ctx, self.selector)
        variants = value.split(';')
        keys = ctx.plural_categories
        last_index = min(len(variants), len(keys)) - 1

        def createVariant(zipped_enum):
            index, (key, variant) = zipped_enum
            # Run the legacy variant through `foreach` which returns an
            # `FTL.Node` describing the transformation required for each
            # variant.  Then evaluate it to a migrated FTL node.
            value = evaluate(ctx, self.foreach(variant))
            return FTL.Variant(
                key=FTL.Symbol(key),
                value=value,
                default=index == last_index
            )

        select = FTL.SelectExpression(
            expression=selector,
            variants=map(createVariant, enumerate(zip(keys, variants)))
        )

        placeable = FTL.Placeable(select)
        return FTL.Pattern([placeable])


class CONCAT(Transform):
    """Concatenate elements of many patterns."""

    def __init__(self, *patterns):
        self.patterns = list(patterns)

    def __call__(self, ctx):
        # Flatten the list of patterns of which each has a list of elements.
        def concat_elements(acc, cur):
            if isinstance(cur, FTL.Pattern):
                acc.extend(cur.elements)
                return acc
            elif (isinstance(cur, FTL.TextElement) or
                  isinstance(cur, FTL.Placeable)):
                acc.append(cur)
                return acc

            raise RuntimeError(
                'CONCAT accepts FTL Patterns and Expressions.'
            )

        # Merge adjecent `FTL.TextElement` nodes.
        def merge_adjecent_text(acc, cur):
            if type(cur) == FTL.TextElement and len(acc):
                last = acc[-1]
                if type(last) == FTL.TextElement:
                    last.value += cur.value
                else:
                    acc.append(cur)
            else:
                acc.append(cur)
            return acc

        elements = reduce(concat_elements, self.patterns, [])
        elements = reduce(merge_adjecent_text, elements, [])
        return FTL.Pattern(elements)

    def traverse(self, fun):
        def visit(value):
            if isinstance(value, FTL.BaseNode):
                return value.traverse(fun)
            if isinstance(value, list):
                return fun(map(visit, value))
            else:
                return fun(value)

        node = self.__class__(
            *[
                visit(value) for value in self.patterns
            ]
        )

        return fun(node)

    def to_json(self):
        def to_json(value):
            if isinstance(value, FTL.BaseNode):
                return value.to_json()
            else:
                return value

        return {
            'type': self.__class__.__name__,
            'patterns': [
                to_json(value) for value in self.patterns
            ]
        }
