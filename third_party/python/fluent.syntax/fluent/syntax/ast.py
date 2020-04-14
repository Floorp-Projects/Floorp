# coding=utf-8
from __future__ import unicode_literals
import re
import sys
import json
import six


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


def to_json(value, fn=None):
    if isinstance(value, BaseNode):
        return value.to_json(fn)
    if isinstance(value, list):
        return list(to_json(item, fn) for item in value)
    if isinstance(value, tuple):
        return list(to_json(item, fn) for item in value)
    else:
        return value


def from_json(value):
    if isinstance(value, dict):
        cls = getattr(sys.modules[__name__], value['type'])
        args = {
            k: from_json(v)
            for k, v in value.items()
            if k != 'type'
        }
        return cls(**args)
    if isinstance(value, list):
        return list(map(from_json, value))
    else:
        return value


def scalars_equal(node1, node2, ignored_fields):
    """Compare two nodes which are not lists."""

    if type(node1) != type(node2):
        return False

    if isinstance(node1, BaseNode):
        return node1.equals(node2, ignored_fields)

    return node1 == node2


class BaseNode(object):
    """Base class for all Fluent AST nodes.

    All productions described in the ASDL subclass BaseNode, including Span and
    Annotation.  Implements __str__, to_json and traverse.
    """

    def traverse(self, fun):
        """DEPRECATED. Please use Visitor or Transformer.

        Postorder-traverse this node and apply `fun` to all child nodes.

        Traverse this node depth-first applying `fun` to subnodes and leaves.
        Children are processed before parents (postorder traversal).

        Return a new instance of the node.
        """

        def visit(value):
            """Call `fun` on `value` and its descendants."""
            if isinstance(value, BaseNode):
                return value.traverse(fun)
            if isinstance(value, list):
                return fun(list(map(visit, value)))
            else:
                return fun(value)

        # Use all attributes found on the node as kwargs to the constructor.
        kwargs = vars(self).items()
        node = self.__class__(
            **{name: visit(value) for name, value in kwargs})

        return fun(node)

    def clone(self):
        """Create a deep clone of the current node."""
        def visit(value):
            """Clone node and its descendants."""
            if isinstance(value, BaseNode):
                return value.clone()
            if isinstance(value, list):
                return [visit(child) for child in value]
            if isinstance(value, tuple):
                return tuple(visit(child) for child in value)
            return value

        # Use all attributes found on the node as kwargs to the constructor.
        return self.__class__(
            **{name: visit(value) for name, value in vars(self).items()}
        )

    def equals(self, other, ignored_fields=['span']):
        """Compare two nodes.

        Nodes are deeply compared on a field by field basis. If possible, False
        is returned early. When comparing attributes and variants in
        SelectExpressions, the order doesn't matter. By default, spans are not
        taken into account.
        """

        self_keys = set(vars(self).keys())
        other_keys = set(vars(other).keys())

        if ignored_fields:
            for key in ignored_fields:
                self_keys.discard(key)
                other_keys.discard(key)

        if self_keys != other_keys:
            return False

        for key in self_keys:
            field1 = getattr(self, key)
            field2 = getattr(other, key)

            # List-typed nodes are compared item-by-item.  When comparing
            # attributes and variants, the order of items doesn't matter.
            if isinstance(field1, list) and isinstance(field2, list):
                if len(field1) != len(field2):
                    return False

                for elem1, elem2 in zip(field1, field2):
                    if not scalars_equal(elem1, elem2, ignored_fields):
                        return False

            elif not scalars_equal(field1, field2, ignored_fields):
                return False

        return True

    def to_json(self, fn=None):
        obj = {
            name: to_json(value, fn)
            for name, value in vars(self).items()
        }
        obj.update(
            {'type': self.__class__.__name__}
        )
        return fn(obj) if fn else obj

    def __str__(self):
        return json.dumps(self.to_json())


class SyntaxNode(BaseNode):
    """Base class for AST nodes which can have Spans."""

    def __init__(self, span=None, **kwargs):
        super(SyntaxNode, self).__init__(**kwargs)
        self.span = span

    def add_span(self, start, end):
        self.span = Span(start, end)


class Resource(SyntaxNode):
    def __init__(self, body=None, **kwargs):
        super(Resource, self).__init__(**kwargs)
        self.body = body or []


class Entry(SyntaxNode):
    """An abstract base class for useful elements of Resource.body."""


class Message(Entry):
    def __init__(self, id, value=None, attributes=None,
                 comment=None, **kwargs):
        super(Message, self).__init__(**kwargs)
        self.id = id
        self.value = value
        self.attributes = attributes or []
        self.comment = comment


class Term(Entry):
    def __init__(self, id, value, attributes=None,
                 comment=None, **kwargs):
        super(Term, self).__init__(**kwargs)
        self.id = id
        self.value = value
        self.attributes = attributes or []
        self.comment = comment


class Pattern(SyntaxNode):
    def __init__(self, elements, **kwargs):
        super(Pattern, self).__init__(**kwargs)
        self.elements = elements


class PatternElement(SyntaxNode):
    """An abstract base class for elements of Patterns."""


class TextElement(PatternElement):
    def __init__(self, value, **kwargs):
        super(TextElement, self).__init__(**kwargs)
        self.value = value


class Placeable(PatternElement):
    def __init__(self, expression, **kwargs):
        super(Placeable, self).__init__(**kwargs)
        self.expression = expression


class Expression(SyntaxNode):
    """An abstract base class for expressions."""


class Literal(Expression):
    """An abstract base class for literals."""
    def __init__(self, value, **kwargs):
        super(Literal, self).__init__(**kwargs)
        self.value = value

    def parse(self):
        return {'value': self.value}


class StringLiteral(Literal):
    def parse(self):
        def from_escape_sequence(matchobj):
            c, codepoint4, codepoint6 = matchobj.groups()
            if c:
                return c
            codepoint = int(codepoint4 or codepoint6, 16)
            if codepoint <= 0xD7FF or 0xE000 <= codepoint:
                return six.unichr(codepoint)
            # Escape sequences reresenting surrogate code points are
            # well-formed but invalid in Fluent. Replace them with U+FFFD
            # REPLACEMENT CHARACTER.
            return '�'

        value = re.sub(
            r'\\(?:(\\|")|u([0-9a-fA-F]{4})|U([0-9a-fA-F]{6}))',
            from_escape_sequence,
            self.value
        )
        return {'value': value}


class NumberLiteral(Literal):
    def parse(self):
        value = float(self.value)
        decimal_position = self.value.find('.')
        precision = 0
        if decimal_position >= 0:
            precision = len(self.value) - decimal_position - 1
        return {
            'value': value,
            'precision': precision
        }


class MessageReference(Expression):
    def __init__(self, id, attribute=None, **kwargs):
        super(MessageReference, self).__init__(**kwargs)
        self.id = id
        self.attribute = attribute


class TermReference(Expression):
    def __init__(self, id, attribute=None, arguments=None, **kwargs):
        super(TermReference, self).__init__(**kwargs)
        self.id = id
        self.attribute = attribute
        self.arguments = arguments


class VariableReference(Expression):
    def __init__(self, id, **kwargs):
        super(VariableReference, self).__init__(**kwargs)
        self.id = id


class FunctionReference(Expression):
    def __init__(self, id, arguments, **kwargs):
        super(FunctionReference, self).__init__(**kwargs)
        self.id = id
        self.arguments = arguments


class SelectExpression(Expression):
    def __init__(self, selector, variants, **kwargs):
        super(SelectExpression, self).__init__(**kwargs)
        self.selector = selector
        self.variants = variants


class CallArguments(SyntaxNode):
    def __init__(self, positional=None, named=None, **kwargs):
        super(CallArguments, self).__init__(**kwargs)
        self.positional = [] if positional is None else positional
        self.named = [] if named is None else named


class Attribute(SyntaxNode):
    def __init__(self, id, value, **kwargs):
        super(Attribute, self).__init__(**kwargs)
        self.id = id
        self.value = value


class Variant(SyntaxNode):
    def __init__(self, key, value, default=False, **kwargs):
        super(Variant, self).__init__(**kwargs)
        self.key = key
        self.value = value
        self.default = default


class NamedArgument(SyntaxNode):
    def __init__(self, name, value, **kwargs):
        super(NamedArgument, self).__init__(**kwargs)
        self.name = name
        self.value = value


class Identifier(SyntaxNode):
    def __init__(self, name, **kwargs):
        super(Identifier, self).__init__(**kwargs)
        self.name = name


class BaseComment(Entry):
    def __init__(self, content=None, **kwargs):
        super(BaseComment, self).__init__(**kwargs)
        self.content = content


class Comment(BaseComment):
    def __init__(self, content=None, **kwargs):
        super(Comment, self).__init__(content, **kwargs)


class GroupComment(BaseComment):
    def __init__(self, content=None, **kwargs):
        super(GroupComment, self).__init__(content, **kwargs)


class ResourceComment(BaseComment):
    def __init__(self, content=None, **kwargs):
        super(ResourceComment, self).__init__(content, **kwargs)


class Junk(SyntaxNode):
    def __init__(self, content=None, annotations=None, **kwargs):
        super(Junk, self).__init__(**kwargs)
        self.content = content
        self.annotations = annotations or []

    def add_annotation(self, annot):
        self.annotations.append(annot)


class Span(BaseNode):
    def __init__(self, start, end, **kwargs):
        super(Span, self).__init__(**kwargs)
        self.start = start
        self.end = end


class Annotation(SyntaxNode):
    def __init__(self, code, arguments=None, message=None, **kwargs):
        super(Annotation, self).__init__(**kwargs)
        self.code = code
        self.arguments = arguments or []
        self.message = message
