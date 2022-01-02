# coding=utf-8
from __future__ import unicode_literals, absolute_import

from .ast import BaseNode


class Visitor(object):
    '''Read-only visitor pattern.

    Subclass this to gather information from an AST.
    To generally define which nodes not to descend in to, overload
    `generic_visit`.
    To handle specific node types, add methods like `visit_Pattern`.
    If you want to still descend into the children of the node, call
    `generic_visit` of the superclass.
    '''
    def visit(self, node):
        if isinstance(node, list):
            for child in node:
                self.visit(child)
            return
        if not isinstance(node, BaseNode):
            return
        nodename = type(node).__name__
        visit = getattr(self, 'visit_{}'.format(nodename), self.generic_visit)
        visit(node)

    def generic_visit(self, node):
        for propname, propvalue in vars(node).items():
            self.visit(propvalue)


class Transformer(Visitor):
    '''In-place AST Transformer pattern.

    Subclass this to create an in-place modified variant
    of the given AST.
    If you need to keep the original AST around, pass
    a `node.clone()` to the transformer.
    '''
    def visit(self, node):
        if not isinstance(node, BaseNode):
            return node

        nodename = type(node).__name__
        visit = getattr(self, 'visit_{}'.format(nodename), self.generic_visit)
        return visit(node)

    def generic_visit(self, node):
        for propname, propvalue in vars(node).items():
            if isinstance(propvalue, list):
                new_vals = []
                for child in propvalue:
                    new_val = self.visit(child)
                    if new_val is not None:
                        new_vals.append(new_val)
                # in-place manipulation
                propvalue[:] = new_vals
            elif isinstance(propvalue, BaseNode):
                new_val = self.visit(propvalue)
                if new_val is None:
                    delattr(node, propname)
                else:
                    setattr(node, propname, new_val)
        return node
