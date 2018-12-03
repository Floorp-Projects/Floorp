# coding=utf8
"""Fluent AST helpers.

The functions defined in this module offer a shorthand for defining common AST
nodes.

They take a string argument and immediately return a corresponding AST node.
(As opposed to Transforms which are AST nodes on their own and only return the
migrated AST nodes when they are evaluated by a MergeContext.) """

from __future__ import unicode_literals
from __future__ import absolute_import

from fluent.syntax import FluentParser, ast as FTL
from .transforms import Transform, CONCAT, COPY
from .errors import NotSupportedError, InvalidTransformError


def VARIABLE_REFERENCE(name):
    """Create an ExternalArgument expression."""

    return FTL.VariableReference(
        id=FTL.Identifier(name)
    )


def MESSAGE_REFERENCE(name):
    """Create a MessageReference expression."""

    return FTL.MessageReference(
        id=FTL.Identifier(name)
    )


def TERM_REFERENCE(name):
    """Create a TermReference expression."""

    return FTL.TermReference(
        id=FTL.Identifier(name)
    )


def transforms_from(ftl, **substitutions):
    """Parse FTL code into a list of Message nodes with Transforms.

    The FTL may use a fabricated COPY function inside of placeables which
    will be converted into actual COPY migration transform.

        new-key = Hardcoded text { COPY("filepath.dtd", "string.key") }

    For convenience, COPY may also refer to transforms_from's keyword
    arguments via the MessageReference syntax:

        transforms_from(\"""
        new-key = Hardcoded text { COPY(file_dtd, "string.key") }
        \""", file_dtd="very/long/path/to/a/file.dtd")

    """

    IMPLICIT_TRANSFORMS = ("CONCAT",)
    FORBIDDEN_TRANSFORMS = ("PLURALS", "REPLACE", "REPLACE_IN_TEXT")

    def into_argument(node):
        """Convert AST node into an argument to migration transforms."""
        if isinstance(node, FTL.StringLiteral):
            # Special cases for booleans which don't exist in Fluent.
            if node.value == "True":
                return True
            if node.value == "False":
                return False
            return node.value
        if isinstance(node, FTL.MessageReference):
            try:
                return substitutions[node.id.name]
            except KeyError:
                raise InvalidTransformError(
                    "Unknown substitution in COPY: {}".format(
                        node.id.name))
        else:
            raise InvalidTransformError(
                "Invalid argument passed to COPY: {}".format(
                    type(node).__name__))

    def into_transforms(node):
        """Convert AST node into a migration transform."""

        if isinstance(node, FTL.Junk):
            anno = node.annotations[0]
            raise InvalidTransformError(
                "Transform contains parse error: {}, at {}".format(
                    anno.message, anno.span.start))
        if isinstance(node, FTL.CallExpression):
            name = node.callee.name
            if name == "COPY":
                args = (into_argument(arg) for arg in node.positional)
                kwargs = {
                    arg.name.name: into_argument(arg.value)
                    for arg in node.named}
                return COPY(*args, **kwargs)
            if name in IMPLICIT_TRANSFORMS:
                raise NotSupportedError(
                    "{} may not be used with transforms_from(). It runs "
                    "implicitly on all Patterns anyways.".format(name))
            if name in FORBIDDEN_TRANSFORMS:
                raise NotSupportedError(
                    "{} may not be used with transforms_from(). It requires "
                    "additional logic in Python code.".format(name))
        if (isinstance(node, FTL.Placeable)
                and isinstance(node.expression, Transform)):
            # Replace the placeable with the transform it's holding.
            # Transforms evaluate to Patterns which aren't valid Placeable
            # expressions.
            return node.expression
        if isinstance(node, FTL.Pattern):
            # Replace the Pattern with CONCAT which is more accepting of its
            # elements. CONCAT takes PatternElements, Expressions and other
            # Patterns (e.g. returned from evaluating transforms).
            return CONCAT(*node.elements)
        return node

    parser = FluentParser(with_spans=False)
    resource = parser.parse(ftl)
    return resource.traverse(into_transforms).body
