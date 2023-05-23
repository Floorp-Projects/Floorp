import re
import sys
import json
from typing import Any, Callable, Dict, List, TypeVar, Union, cast

Node = TypeVar('Node', bound='BaseNode')
ToJsonFn = Callable[[Dict[str, Any]], Any]


def to_json(value: Any, fn: Union[ToJsonFn, None] = None) -> Any:
    if isinstance(value, BaseNode):
        return value.to_json(fn)
    if isinstance(value, list):
        return list(to_json(item, fn) for item in value)
    if isinstance(value, tuple):
        return list(to_json(item, fn) for item in value)
    else:
        return value


def from_json(value: Any) -> Any:
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


def scalars_equal(node1: Any, node2: Any, ignored_fields: List[str]) -> bool:
    """Compare two nodes which are not lists."""

    if type(node1) != type(node2):
        return False

    if isinstance(node1, BaseNode):
        return node1.equals(node2, ignored_fields)

    return cast(bool, node1 == node2)


class BaseNode:
    """Base class for all Fluent AST nodes.

    All productions described in the ASDL subclass BaseNode, including Span and
    Annotation.  Implements __str__, to_json and traverse.
    """

    def clone(self: Node) -> Node:
        """Create a deep clone of the current node."""
        def visit(value: Any) -> Any:
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

    def equals(self, other: 'BaseNode', ignored_fields: List[str] = ['span']) -> bool:
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

    def to_json(self, fn: Union[ToJsonFn, None] = None) -> Any:
        obj = {
            name: to_json(value, fn)
            for name, value in vars(self).items()
        }
        obj.update(
            {'type': self.__class__.__name__}
        )
        return fn(obj) if fn else obj

    def __str__(self) -> str:
        return json.dumps(self.to_json())


class SyntaxNode(BaseNode):
    """Base class for AST nodes which can have Spans."""

    def __init__(self, span: Union['Span', None] = None, **kwargs: Any):
        super().__init__(**kwargs)
        self.span = span

    def add_span(self, start: int, end: int) -> None:
        self.span = Span(start, end)


class Resource(SyntaxNode):
    def __init__(self, body: Union[List['EntryType'], None] = None, **kwargs: Any):
        super().__init__(**kwargs)
        self.body = body or []


class Entry(SyntaxNode):
    """An abstract base class for useful elements of Resource.body."""


class Message(Entry):
    def __init__(self,
                 id: 'Identifier',
                 value: Union['Pattern', None] = None,
                 attributes: Union[List['Attribute'], None] = None,
                 comment: Union['Comment', None] = None,
                 **kwargs: Any):
        super().__init__(**kwargs)
        self.id = id
        self.value = value
        self.attributes = attributes or []
        self.comment = comment


class Term(Entry):
    def __init__(self, id: 'Identifier', value: 'Pattern', attributes: Union[List['Attribute'], None] = None,
                 comment: Union['Comment', None] = None, **kwargs: Any):
        super().__init__(**kwargs)
        self.id = id
        self.value = value
        self.attributes = attributes or []
        self.comment = comment


class Pattern(SyntaxNode):
    def __init__(self, elements: List[Union['TextElement', 'Placeable']], **kwargs: Any):
        super().__init__(**kwargs)
        self.elements = elements


class PatternElement(SyntaxNode):
    """An abstract base class for elements of Patterns."""


class TextElement(PatternElement):
    def __init__(self, value: str, **kwargs: Any):
        super().__init__(**kwargs)
        self.value = value


class Placeable(PatternElement):
    def __init__(self,
                 expression: Union['InlineExpression', 'Placeable', 'SelectExpression'],
                 **kwargs: Any):
        super().__init__(**kwargs)
        self.expression = expression


class Expression(SyntaxNode):
    """An abstract base class for expressions."""


class Literal(Expression):
    """An abstract base class for literals."""

    def __init__(self, value: str, **kwargs: Any):
        super().__init__(**kwargs)
        self.value = value

    def parse(self) -> Dict[str, Any]:
        return {'value': self.value}


class StringLiteral(Literal):
    def parse(self) -> Dict[str, str]:
        def from_escape_sequence(matchobj: Any) -> str:
            c, codepoint4, codepoint6 = matchobj.groups()
            if c:
                return cast(str, c)
            codepoint = int(codepoint4 or codepoint6, 16)
            if codepoint <= 0xD7FF or 0xE000 <= codepoint:
                return chr(codepoint)
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
    def parse(self) -> Dict[str, Union[float, int]]:
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
    def __init__(self, id: 'Identifier', attribute: Union['Identifier', None] = None, **kwargs: Any):
        super().__init__(**kwargs)
        self.id = id
        self.attribute = attribute


class TermReference(Expression):
    def __init__(self,
                 id: 'Identifier',
                 attribute: Union['Identifier', None] = None,
                 arguments: Union['CallArguments', None] = None,
                 **kwargs: Any):
        super().__init__(**kwargs)
        self.id = id
        self.attribute = attribute
        self.arguments = arguments


class VariableReference(Expression):
    def __init__(self, id: 'Identifier', **kwargs: Any):
        super().__init__(**kwargs)
        self.id = id


class FunctionReference(Expression):
    def __init__(self, id: 'Identifier', arguments: 'CallArguments', **kwargs: Any):
        super().__init__(**kwargs)
        self.id = id
        self.arguments = arguments


class SelectExpression(Expression):
    def __init__(self, selector: 'InlineExpression', variants: List['Variant'], **kwargs: Any):
        super().__init__(**kwargs)
        self.selector = selector
        self.variants = variants


class CallArguments(SyntaxNode):
    def __init__(self,
                 positional: Union[List[Union['InlineExpression', Placeable]], None] = None,
                 named: Union[List['NamedArgument'], None] = None,
                 **kwargs: Any):
        super().__init__(**kwargs)
        self.positional = [] if positional is None else positional
        self.named = [] if named is None else named


class Attribute(SyntaxNode):
    def __init__(self, id: 'Identifier', value: Pattern, **kwargs: Any):
        super().__init__(**kwargs)
        self.id = id
        self.value = value


class Variant(SyntaxNode):
    def __init__(self, key: Union['Identifier', NumberLiteral], value: Pattern, default: bool = False, **kwargs: Any):
        super().__init__(**kwargs)
        self.key = key
        self.value = value
        self.default = default


class NamedArgument(SyntaxNode):
    def __init__(self, name: 'Identifier', value: Union[NumberLiteral, StringLiteral], **kwargs: Any):
        super().__init__(**kwargs)
        self.name = name
        self.value = value


class Identifier(SyntaxNode):
    def __init__(self, name: str, **kwargs: Any):
        super().__init__(**kwargs)
        self.name = name


class BaseComment(Entry):
    def __init__(self, content: Union[str, None] = None, **kwargs: Any):
        super().__init__(**kwargs)
        self.content = content


class Comment(BaseComment):
    def __init__(self, content: Union[str, None] = None, **kwargs: Any):
        super().__init__(content, **kwargs)


class GroupComment(BaseComment):
    def __init__(self, content: Union[str, None] = None, **kwargs: Any):
        super().__init__(content, **kwargs)


class ResourceComment(BaseComment):
    def __init__(self, content: Union[str, None] = None, **kwargs: Any):
        super().__init__(content, **kwargs)


class Junk(SyntaxNode):
    def __init__(self,
                 content: Union[str, None] = None,
                 annotations: Union[List['Annotation'], None] = None,
                 **kwargs: Any):
        super().__init__(**kwargs)
        self.content = content
        self.annotations = annotations or []

    def add_annotation(self, annot: 'Annotation') -> None:
        self.annotations.append(annot)


class Span(BaseNode):
    def __init__(self, start: int, end: int, **kwargs: Any):
        super().__init__(**kwargs)
        self.start = start
        self.end = end


class Annotation(SyntaxNode):
    def __init__(self,
                 code: str,
                 arguments: Union[List[Any], None] = None,
                 message: Union[str, None] = None,
                 **kwargs: Any):
        super().__init__(**kwargs)
        self.code = code
        self.arguments = arguments or []
        self.message = message


EntryType = Union[Message, Term, Comment, GroupComment, ResourceComment, Junk]
InlineExpression = Union[NumberLiteral, StringLiteral, MessageReference,
                         TermReference, VariableReference, FunctionReference]
