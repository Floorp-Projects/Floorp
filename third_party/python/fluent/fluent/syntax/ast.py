from __future__ import unicode_literals
import sys
import json


def to_json(value):
    if isinstance(value, BaseNode):
        return value.to_json()
    if isinstance(value, list):
        return list(map(to_json, value))
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
        """Postorder-traverse this node and apply `fun` to all child nodes.

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

        node = self.__class__(
            **{
                name: visit(value)
                for name, value in vars(self).items()
            }
        )

        return fun(node)

    def equals(self, other, ignored_fields=['span']):
        """Compare two nodes.

        Nodes are deeply compared on a field by field basis. If possible, False
        is returned early. When comparing attributes, tags and variants in
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
            # attributes, tags and variants, the order of items doesn't matter.
            if isinstance(field1, list) and isinstance(field2, list):
                if len(field1) != len(field2):
                    return False

                # These functions are used to sort lists of items for when
                # order doesn't matter.  Annotations are also lists but they
                # can't be keyed on any of their fields reliably.
                field_sorting = {
                    'attributes': lambda elem: elem.id.name,
                    'tags': lambda elem: elem.name.name,
                    'variants': lambda elem: elem.key.name,
                }

                if key in field_sorting:
                    sorting = field_sorting[key]
                    field1 = sorted(field1, key=sorting)
                    field2 = sorted(field2, key=sorting)

                for elem1, elem2 in zip(field1, field2):
                    if not scalars_equal(elem1, elem2, ignored_fields):
                        return False

            elif not scalars_equal(field1, field2, ignored_fields):
                return False

        return True

    def to_json(self):
        obj = {
            name: to_json(value)
            for name, value in vars(self).items()
        }
        obj.update(
            {'type': self.__class__.__name__}
        )
        return obj

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
    def __init__(self, body=None, comment=None, **kwargs):
        super(Resource, self).__init__(**kwargs)
        self.body = body or []
        self.comment = comment


class Entry(SyntaxNode):
    def __init__(self, annotations=None, **kwargs):
        super(Entry, self).__init__(**kwargs)
        self.annotations = annotations or []

    def add_annotation(self, annot):
        self.annotations.append(annot)


class Message(Entry):
    def __init__(self, id, value=None, attributes=None, tags=None,
                 comment=None, **kwargs):
        super(Message, self).__init__(**kwargs)
        self.id = id
        self.value = value
        self.attributes = attributes or []
        self.tags = tags or []
        self.comment = comment

class Pattern(SyntaxNode):
    def __init__(self, elements, **kwargs):
        super(Pattern, self).__init__(**kwargs)
        self.elements = elements

class TextElement(SyntaxNode):
    def __init__(self, value, **kwargs):
        super(TextElement, self).__init__(**kwargs)
        self.value = value

class Placeable(SyntaxNode):
    def __init__(self, expression, **kwargs):
        super(Placeable, self).__init__(**kwargs)
        self.expression = expression

class Expression(SyntaxNode):
    def __init__(self, **kwargs):
        super(Expression, self).__init__(**kwargs)

class StringExpression(Expression):
    def __init__(self, value, **kwargs):
        super(StringExpression, self).__init__(**kwargs)
        self.value = value

class NumberExpression(Expression):
    def __init__(self, value, **kwargs):
        super(NumberExpression, self).__init__(**kwargs)
        self.value = value

class MessageReference(Expression):
    def __init__(self, id, **kwargs):
        super(MessageReference, self).__init__(**kwargs)
        self.id = id

class ExternalArgument(Expression):
    def __init__(self, id, **kwargs):
        super(ExternalArgument, self).__init__(**kwargs)
        self.id = id

class SelectExpression(Expression):
    def __init__(self, expression, variants, **kwargs):
        super(SelectExpression, self).__init__(**kwargs)
        self.expression = expression
        self.variants = variants

class AttributeExpression(Expression):
    def __init__(self, id, name, **kwargs):
        super(AttributeExpression, self).__init__(**kwargs)
        self.id = id
        self.name = name

class VariantExpression(Expression):
    def __init__(self, id, key, **kwargs):
        super(VariantExpression, self).__init__(**kwargs)
        self.id = id
        self.key = key

class CallExpression(Expression):
    def __init__(self, callee, args, **kwargs):
        super(CallExpression, self).__init__(**kwargs)
        self.callee = callee
        self.args = args

class Attribute(SyntaxNode):
    def __init__(self, id, value, **kwargs):
        super(Attribute, self).__init__(**kwargs)
        self.id = id
        self.value = value

class Tag(SyntaxNode):
    def __init__(self, name, **kwargs):
        super(Tag, self).__init__(**kwargs)
        self.name = name

class Variant(SyntaxNode):
    def __init__(self, key, value, default=False, **kwargs):
        super(Variant, self).__init__(**kwargs)
        self.key = key
        self.value = value
        self.default = default

class NamedArgument(SyntaxNode):
    def __init__(self, name, val, **kwargs):
        super(NamedArgument, self).__init__(**kwargs)
        self.name = name
        self.val = val

class Identifier(SyntaxNode):
    def __init__(self, name, **kwargs):
        super(Identifier, self).__init__(**kwargs)
        self.name = name

class Symbol(Identifier):
    def __init__(self, name, **kwargs):
        super(Symbol, self).__init__(name, **kwargs)

class Comment(Entry):
    def __init__(self, content=None, **kwargs):
        super(Comment, self).__init__(**kwargs)
        self.content = content

class Section(Entry):
    def __init__(self, name, comment=None, **kwargs):
        super(Section, self).__init__(**kwargs)
        self.name = name
        self.comment = comment

class Function(Identifier):
    def __init__(self, name, **kwargs):
        super(Function, self).__init__(name, **kwargs)

class Junk(Entry):
    def __init__(self, content=None, **kwargs):
        super(Junk, self).__init__(**kwargs)
        self.content = content


class Span(BaseNode):
    def __init__(self, start, end, **kwargs):
        super(Span, self).__init__(**kwargs)
        self.start = start
        self.end = end


class Annotation(SyntaxNode):
    def __init__(self, code, args=None, message=None, **kwargs):
        super(Annotation, self).__init__(**kwargs)
        self.code = code
        self.args = args or []
        self.message = message
