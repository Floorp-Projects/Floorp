from typing import List, Union
from . import ast


def indent_except_first_line(content: str) -> str:
    return "    ".join(
        content.splitlines(True)
    )


def includes_new_line(elem: Union[ast.TextElement, ast.Placeable]) -> bool:
    return isinstance(elem, ast.TextElement) and "\n" in elem.value


def is_select_expr(elem: Union[ast.TextElement, ast.Placeable]) -> bool:
    return (
        isinstance(elem, ast.Placeable) and
        isinstance(elem.expression, ast.SelectExpression))


def should_start_on_new_line(pattern: ast.Pattern) -> bool:
    is_multiline = any(is_select_expr(elem) for elem in pattern.elements) \
        or any(includes_new_line(elem) for elem in pattern.elements)

    if is_multiline:
        first_element = pattern.elements[0]
        if isinstance(first_element, ast.TextElement):
            first_char = first_element.value[0]
            if first_char in ("[", ".", "*"):
                return False
        return True
    return False


class FluentSerializer:
    """FluentSerializer converts :class:`.ast.SyntaxNode` objects to unicode strings.

    `with_junk` controls if parse errors are written back or not.
    """
    HAS_ENTRIES = 1

    def __init__(self, with_junk: bool = False):
        self.with_junk = with_junk

    def serialize(self, resource: ast.Resource) -> str:
        "Serialize a :class:`.ast.Resource` to a string."
        if not isinstance(resource, ast.Resource):
            raise Exception('Unknown resource type: {}'.format(type(resource)))

        state = 0

        parts: List[str] = []
        for entry in resource.body:
            if not isinstance(entry, ast.Junk) or self.with_junk:
                parts.append(self.serialize_entry(entry, state))
                if not state & self.HAS_ENTRIES:
                    state |= self.HAS_ENTRIES

        return "".join(parts)

    def serialize_entry(self, entry: ast.EntryType, state: int = 0) -> str:
        "Serialize an :class:`.ast.Entry` to a string."
        if isinstance(entry, ast.Message):
            return serialize_message(entry)
        if isinstance(entry, ast.Term):
            return serialize_term(entry)
        if isinstance(entry, ast.Comment):
            if state & self.HAS_ENTRIES:
                return "\n{}\n".format(serialize_comment(entry, "#"))
            return "{}\n".format(serialize_comment(entry, "#"))
        if isinstance(entry, ast.GroupComment):
            if state & self.HAS_ENTRIES:
                return "\n{}\n".format(serialize_comment(entry, "##"))
            return "{}\n".format(serialize_comment(entry, "##"))
        if isinstance(entry, ast.ResourceComment):
            if state & self.HAS_ENTRIES:
                return "\n{}\n".format(serialize_comment(entry, "###"))
            return "{}\n".format(serialize_comment(entry, "###"))
        if isinstance(entry, ast.Junk):
            return serialize_junk(entry)
        raise Exception('Unknown entry type: {}'.format(type(entry)))


def serialize_comment(comment: Union[ast.Comment, ast.GroupComment, ast.ResourceComment], prefix: str = "#") -> str:
    if not comment.content:
        return f'{prefix}\n'

    prefixed = "\n".join([
        prefix if len(line) == 0 else f"{prefix} {line}"
        for line in comment.content.split("\n")
    ])
    # Add the trailing line break.
    return f'{prefixed}\n'


def serialize_junk(junk: ast.Junk) -> str:
    return junk.content or ''


def serialize_message(message: ast.Message) -> str:
    parts: List[str] = []

    if message.comment:
        parts.append(serialize_comment(message.comment))

    parts.append(f"{message.id.name} =")

    if message.value:
        parts.append(serialize_pattern(message.value))

    if message.attributes:
        for attribute in message.attributes:
            parts.append(serialize_attribute(attribute))

    parts.append("\n")
    return ''.join(parts)


def serialize_term(term: ast.Term) -> str:
    parts: List[str] = []

    if term.comment:
        parts.append(serialize_comment(term.comment))

    parts.append(f"-{term.id.name} =")
    parts.append(serialize_pattern(term.value))

    if term.attributes:
        for attribute in term.attributes:
            parts.append(serialize_attribute(attribute))

    parts.append("\n")
    return ''.join(parts)


def serialize_attribute(attribute: ast.Attribute) -> str:
    return "\n    .{} ={}".format(
        attribute.id.name,
        indent_except_first_line(serialize_pattern(attribute.value))
    )


def serialize_pattern(pattern: ast.Pattern) -> str:
    content = "".join(serialize_element(elem) for elem in pattern.elements)
    content = indent_except_first_line(content)

    if should_start_on_new_line(pattern):
        return f'\n    {content}'

    return f' {content}'


def serialize_element(element: ast.PatternElement) -> str:
    if isinstance(element, ast.TextElement):
        return element.value
    if isinstance(element, ast.Placeable):
        return serialize_placeable(element)
    raise Exception('Unknown element type: {}'.format(type(element)))


def serialize_placeable(placeable: ast.Placeable) -> str:
    expr = placeable.expression
    if isinstance(expr, ast.Placeable):
        return "{{{}}}".format(serialize_placeable(expr))
    if isinstance(expr, ast.SelectExpression):
        # Special-case select expressions to control the withespace around the
        # opening and the closing brace.
        return "{{ {}}}".format(serialize_expression(expr))
    if isinstance(expr, ast.Expression):
        return "{{ {} }}".format(serialize_expression(expr))
    raise Exception('Unknown expression type: {}'.format(type(expr)))


def serialize_expression(expression: Union[ast.Expression, ast.Placeable]) -> str:
    if isinstance(expression, ast.StringLiteral):
        return f'"{expression.value}"'
    if isinstance(expression, ast.NumberLiteral):
        return expression.value
    if isinstance(expression, ast.VariableReference):
        return f"${expression.id.name}"
    if isinstance(expression, ast.TermReference):
        out = f"-{expression.id.name}"
        if expression.attribute is not None:
            out += f".{expression.attribute.name}"
        if expression.arguments is not None:
            out += serialize_call_arguments(expression.arguments)
        return out
    if isinstance(expression, ast.MessageReference):
        out = expression.id.name
        if expression.attribute is not None:
            out += f".{expression.attribute.name}"
        return out
    if isinstance(expression, ast.FunctionReference):
        args = serialize_call_arguments(expression.arguments)
        return f"{expression.id.name}{args}"
    if isinstance(expression, ast.SelectExpression):
        out = "{} ->".format(
            serialize_expression(expression.selector))
        for variant in expression.variants:
            out += serialize_variant(variant)
        return f"{out}\n"
    if isinstance(expression, ast.Placeable):
        return serialize_placeable(expression)
    raise Exception('Unknown expression type: {}'.format(type(expression)))


def serialize_variant(variant: ast.Variant) -> str:
    return "\n{}[{}]{}".format(
        "   *" if variant.default else "    ",
        serialize_variant_key(variant.key),
        indent_except_first_line(serialize_pattern(variant.value))
    )


def serialize_call_arguments(expr: ast.CallArguments) -> str:
    positional = ", ".join(
        serialize_expression(arg) for arg in expr.positional)
    named = ", ".join(
        serialize_named_argument(arg) for arg in expr.named)
    if len(expr.positional) > 0 and len(expr.named) > 0:
        return f'({positional}, {named})'
    return '({})'.format(positional or named)


def serialize_named_argument(arg: ast.NamedArgument) -> str:
    return "{}: {}".format(
        arg.name.name,
        serialize_expression(arg.value)
    )


def serialize_variant_key(key: Union[ast.Identifier, ast.NumberLiteral]) -> str:
    if isinstance(key, ast.Identifier):
        return key.name
    if isinstance(key, ast.NumberLiteral):
        return key.value
    raise Exception('Unknown variant key type: {}'.format(type(key)))
