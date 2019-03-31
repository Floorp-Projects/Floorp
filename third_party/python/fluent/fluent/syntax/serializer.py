from __future__ import unicode_literals
from . import ast


def indent(content):
    return "    ".join(
        content.splitlines(True)
    )


def includes_new_line(elem):
    return isinstance(elem, ast.TextElement) and "\n" in elem.value


def is_select_expr(elem):
    return (
        isinstance(elem, ast.Placeable) and
        isinstance(elem.expression, ast.SelectExpression))


class FluentSerializer(object):
    HAS_ENTRIES = 1

    def __init__(self, with_junk=False):
        self.with_junk = with_junk

    def serialize(self, resource):
        if not isinstance(resource, ast.Resource):
            raise Exception('Unknown resource type: {}'.format(type(resource)))

        state = 0

        parts = []
        for entry in resource.body:
            if not isinstance(entry, ast.Junk) or self.with_junk:
                parts.append(self.serialize_entry(entry, state))
                if not state & self.HAS_ENTRIES:
                    state |= self.HAS_ENTRIES

        return "".join(parts)

    def serialize_entry(self, entry, state=0):
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


def serialize_comment(comment, prefix="#"):
    prefixed = "\n".join([
        prefix if len(line) == 0 else "{} {}".format(prefix, line)
        for line in comment.content.splitlines(False)
    ])
    # Add the trailing line break.
    return '{}\n'.format(prefixed)


def serialize_junk(junk):
    return junk.content


def serialize_message(message):
    parts = []

    if message.comment:
        parts.append(serialize_comment(message.comment))

    parts.append("{} =".format(message.id.name))

    if message.value:
        parts.append(serialize_pattern(message.value))

    if message.attributes:
        for attribute in message.attributes:
            parts.append(serialize_attribute(attribute))

    parts.append("\n")
    return ''.join(parts)


def serialize_term(term):
    parts = []

    if term.comment:
        parts.append(serialize_comment(term.comment))

    parts.append("-{} =".format(term.id.name))
    parts.append(serialize_pattern(term.value))

    if term.attributes:
        for attribute in term.attributes:
            parts.append(serialize_attribute(attribute))

    parts.append("\n")
    return ''.join(parts)


def serialize_attribute(attribute):
    return "\n    .{} ={}".format(
        attribute.id.name,
        indent(serialize_pattern(attribute.value))
    )


def serialize_pattern(pattern):
    content = "".join([
        serialize_element(elem)
        for elem in pattern.elements])
    start_on_new_line = any(
        includes_new_line(elem) or is_select_expr(elem)
        for elem in pattern.elements)
    if start_on_new_line:
        return '\n    {}'.format(indent(content))

    return ' {}'.format(content)


def serialize_element(element):
    if isinstance(element, ast.TextElement):
        return element.value
    if isinstance(element, ast.Placeable):
        return serialize_placeable(element)
    raise Exception('Unknown element type: {}'.format(type(element)))


def serialize_placeable(placeable):
    expr = placeable.expression
    if isinstance(expr, ast.Placeable):
        return "{{{}}}".format(serialize_placeable(expr))
    if isinstance(expr, ast.SelectExpression):
        # Special-case select expressions to control the withespace around the
        # opening and the closing brace.
        return "{{ {}}}".format(serialize_expression(expr))
    if isinstance(expr, ast.Expression):
        return "{{ {} }}".format(serialize_expression(expr))


def serialize_expression(expression):
    if isinstance(expression, ast.StringLiteral):
        return '"{}"'.format(expression.value)
    if isinstance(expression, ast.NumberLiteral):
        return expression.value
    if isinstance(expression, ast.VariableReference):
        return "${}".format(expression.id.name)
    if isinstance(expression, ast.TermReference):
        out = "-{}".format(expression.id.name)
        if expression.attribute is not None:
            out += ".{}".format(expression.attribute.name)
        if expression.arguments is not None:
            out += serialize_call_arguments(expression.arguments)
        return out
    if isinstance(expression, ast.MessageReference):
        out = expression.id.name
        if expression.attribute is not None:
            out += ".{}".format(expression.attribute.name)
        return out
    if isinstance(expression, ast.FunctionReference):
        args = serialize_call_arguments(expression.arguments)
        return "{}{}".format(expression.id.name, args)
    if isinstance(expression, ast.SelectExpression):
        out = "{} ->".format(
            serialize_expression(expression.selector))
        for variant in expression.variants:
            out += serialize_variant(variant)
        return "{}\n".format(out)
    if isinstance(expression, ast.Placeable):
        return serialize_placeable(expression)
    raise Exception('Unknown expression type: {}'.format(type(expression)))


def serialize_variant(variant):
    return "\n{}[{}]{}".format(
        "   *" if variant.default else "    ",
        serialize_variant_key(variant.key),
        indent(serialize_pattern(variant.value))
    )


def serialize_call_arguments(expr):
    positional = ", ".join(
        serialize_expression(arg) for arg in expr.positional)
    named = ", ".join(
        serialize_named_argument(arg) for arg in expr.named)
    if len(expr.positional) > 0 and len(expr.named) > 0:
        return '({}, {})'.format(positional, named)
    return '({})'.format(positional or named)


def serialize_named_argument(arg):
    return "{}: {}".format(
        arg.name.name,
        serialize_expression(arg.value)
    )


def serialize_variant_key(key):
    if isinstance(key, ast.Identifier):
        return key.name
    if isinstance(key, ast.NumberLiteral):
        return key.value
    raise Exception('Unknown variant key type: {}'.format(type(key)))
