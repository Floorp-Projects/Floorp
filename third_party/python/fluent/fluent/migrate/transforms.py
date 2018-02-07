# coding=utf8
"""Migration Transforms.

Transforms are AST nodes which describe how legacy translations should be
migrated.  They are created inert and only return the migrated AST nodes when
they are evaluated by a MergeContext.

All Transforms evaluate to Fluent Patterns. This makes them suitable for
defining migrations of values of message, attributes and variants.  The special
CONCAT Transform is capable of joining multiple Patterns returned by evaluating
other Transforms into a single Pattern.  It can also concatenate Pattern
elements: TextElements and Placeables.

The COPY, REPLACE and PLURALS Transforms inherit from Source which is a special
AST Node defining the location (the file path and the id) of the legacy
translation.  During the migration, the current MergeContext scans the
migration spec for Source nodes and extracts the information about all legacy
translations being migrated. For instance,

    COPY('file.dtd', 'hello')

is equivalent to:

    FTL.Pattern([
        FTL.TextElement(Source('file.dtd', 'hello'))
    ])

Sometimes it's useful to work with text rather than (path, key) source
definitions.  This is the case when the migrated translation requires some
hardcoded text, e.g. <a> and </a> when multiple translations become a single
one with a DOM overlay. In such cases it's best to use the AST nodes:

    FTL.Message(
        id=FTL.Identifier('update-failed'),
        value=CONCAT(
            COPY('aboutDialog.dtd', 'update.failed.start'),
            FTL.TextElement('<a>'),
            COPY('aboutDialog.dtd', 'update.failed.linkText'),
            FTL.TextElement('</a>'),
            COPY('aboutDialog.dtd', 'update.failed.end'),
        )
    )

The REPLACE_IN_TEXT Transform also takes text as input, making in possible to
pass it as the foreach function of the PLURALS Transform.  In this case, each
slice of the plural string will be run through a REPLACE_IN_TEXT operation.
Those slices are strings, so a REPLACE(path, key, …) wouldn't be suitable for
them.

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
import itertools

import fluent.syntax.ast as FTL
from .errors import NotSupportedError


def pattern_from_text(value):
    return FTL.Pattern([
        FTL.TextElement(value)
    ])


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

    @staticmethod
    def flatten_elements(elements):
        '''Flatten a list of FTL nodes into valid Pattern's elements'''
        flattened = []
        for element in elements:
            if isinstance(element, FTL.Pattern):
                flattened.extend(element.elements)
            elif isinstance(element, FTL.PatternElement):
                flattened.append(element)
            elif isinstance(element, FTL.Expression):
                flattened.append(FTL.Placeable(element))
            else:
                raise RuntimeError(
                    'Expected Pattern, PatternElement or Expression')
        return flattened

    @staticmethod
    def prune_text_elements(elements):
        '''Join adjacent TextElements and remove empty ones'''
        pruned = []
        # Group elements in contiguous sequences of the same type.
        for elem_type, elems in itertools.groupby(elements, key=type):
            if elem_type is FTL.TextElement:
                # Join adjacent TextElements.
                text = FTL.TextElement(''.join(elem.value for elem in elems))
                # And remove empty ones.
                if len(text.value) > 0:
                    pruned.append(text)
            else:
                pruned.extend(elems)
        return pruned


class Source(Transform):
    """Declare the source translation to be migrated with other transforms.

    When evaluated, `Source` returns a simple string value. Escaped characters
    are unescaped by the compare-locales parser according to the file format:

      - in properties files: \\uXXXX,
      - in DTD files: known named, decimal, and hexadecimal HTML entities.

    Consult the following files for the list of known named HTML entities:

    https://github.com/python/cpython/blob/2.7/Lib/htmlentitydefs.py
    https://github.com/python/cpython/blob/3.6/Lib/html/entities.py

    """

    def __init__(self, path, key):
        if path.endswith('.ftl'):
            raise NotSupportedError(
                'Migrating translations from Fluent files is not supported '
                '({})'.format(path))

        self.path = path
        self.key = key

    def __call__(self, ctx):
        return ctx.get_source(self.path, self.key)


class COPY(Source):
    """Create a Pattern with the translation value from the given source."""

    def __call__(self, ctx):
        source = super(self.__class__, self).__call__(ctx)
        return pattern_from_text(source)


class REPLACE_IN_TEXT(Transform):
    """Replace various placeables in the translation with FTL.

    The original placeables are defined as keys on the `replacements` dict.
    For each key the value is defined as a FTL Pattern, Placeable,
    TextElement or Expressions to be interpolated.
    """

    def __init__(self, value, replacements):
        self.value = value
        self.replacements = replacements

    def __call__(self, ctx):

        # Only replace placeables which are present in the translation.
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

        # A list of PatternElements built from the legacy translation and the
        # FTL replacements. It may contain empty or adjacent TextElements.
        elements = []
        tail = self.value

        # Convert original placeables and text into FTL Nodes. For each
        # original placeable the translation will be partitioned around it and
        # the text before it will be converted into an `FTL.TextElement` and
        # the placeable will be replaced with its replacement.
        for key in keys_in_order:
            before, key, tail = tail.partition(key)
            elements.append(FTL.TextElement(before))
            elements.append(replacements[key])

        # Dont' forget about the tail after the loop ends.
        elements.append(FTL.TextElement(tail))

        elements = self.flatten_elements(elements)
        elements = self.prune_text_elements(elements)
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
    which should return an `FTL.Node` or a `Transform`. By default, the
    `foreach` function transforms the source text into a Pattern with a single
    TextElement.
    """

    def __init__(self, path, key, selector, foreach=pattern_from_text):
        super(self.__class__, self).__init__(path, key)
        self.selector = selector
        self.foreach = foreach

    def __call__(self, ctx):
        value = super(self.__class__, self).__call__(ctx)
        selector = evaluate(ctx, self.selector)
        variants = value.split(';')
        keys = ctx.plural_categories

        # A special case for languages with one plural category. We don't need
        # to insert a SelectExpression at all for them.
        if len(keys) == len(variants) == 1:
            variant, = variants
            return evaluate(ctx, self.foreach(variant))

        last_index = min(len(variants), len(keys)) - 1

        def createVariant(zipped_enum):
            index, (key, variant) = zipped_enum
            # Run the legacy variant through `foreach` which returns an
            # `FTL.Node` describing the transformation required for each
            # variant.  Then evaluate it to a migrated FTL node.
            value = evaluate(ctx, self.foreach(variant))
            return FTL.Variant(
                key=FTL.VariantName(key),
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
    """Create a new Pattern from Patterns, PatternElements and Expressions."""

    def __init__(self, *elements, **kwargs):
        # We want to support both passing elements as *elements in the
        # migration specs and as elements=[]. The latter is used by
        # FTL.BaseNode.traverse when it recreates the traversed node using its
        # attributes as kwargs.
        self.elements = list(kwargs.get('elements', elements))

    def __call__(self, ctx):
        elements = self.flatten_elements(self.elements)
        elements = self.prune_text_elements(elements)
        return FTL.Pattern(elements)
