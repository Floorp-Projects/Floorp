# coding=utf8
"""Fluent AST helpers.

The functions defined in this module offer a shorthand for defining common AST
nodes.

They take a string argument and immediately return a corresponding AST node.
(As opposed to Transforms which are AST nodes on their own and only return the
migrated AST nodes when they are evaluated by a MergeContext.) """

from __future__ import unicode_literals

import fluent.syntax.ast as FTL


def EXTERNAL_ARGUMENT(name):
    """Create an ExternalArgument expression."""

    return FTL.ExternalArgument(
        id=FTL.Identifier(name)
    )


def MESSAGE_REFERENCE(name):
    """Create a MessageReference expression."""

    return FTL.MessageReference(
        id=FTL.Identifier(name)
    )
