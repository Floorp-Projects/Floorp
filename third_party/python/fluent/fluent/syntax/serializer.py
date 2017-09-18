from __future__ import unicode_literals
from . import ast


def indent(content):
    return "    ".join(
        content.splitlines(True)
    )


def contain_new_line(elems):
    return bool([
        elem for elem in elems
        if isinstance(elem, ast.TextElement) and "\n" in elem.value
    ])


class FluentSerializer(object):
    def __init__(self, with_junk=False):
        self.with_junk = with_junk

    def serialize(self, resource):
        parts = []
        if resource.comment:
            parts.append(
                "{}\n\n".format(
                    serialize_comment(resource.comment)
                )
            )
        for entry in resource.body:
            if not isinstance(entry, ast.Junk) or self.with_junk:
                parts.append(self.serialize_entry(entry))

        return "".join(parts)

    def serialize_entry(self, entry):
        if isinstance(entry, ast.Message):
            return serialize_message(entry)
        if isinstance(entry, ast.Section):
            return serialize_section(entry)
        if isinstance(entry, ast.Comment):
            return "\n{}\n\n".format(serialize_comment(entry))
        if isinstance(entry, ast.Junk):
            return serialize_junk(entry)
        raise Exception('Unknown entry type: {}'.format(entry.type))


def serialize_comment(comment):
    return "".join([
        "{}{}".format("// ", line)
        for line in comment.content.splitlines(True)
    ])


def serialize_section(section):
    if section.comment:
        return "\n\n{}\n[[ {} ]]\n\n".format(
            serialize_comment(section.comment),
            serialize_symbol(section.name)
        )
    else:
        return "\n\n[[ {} ]]\n\n".format(
            serialize_symbol(section.name)
        )


def serialize_junk(junk):
    return junk.content


def serialize_message(message):
    parts = []

    if message.comment:
        parts.append(serialize_comment(message.comment))
        parts.append("\n")

    parts.append(serialize_identifier(message.id))

    if message.value:
        parts.append(" =")
        parts.append(serialize_value(message.value))

    if message.tags:
        for tag in message.tags:
            parts.append(serialize_tag(tag))

    if message.attributes:
        for attribute in message.attributes:
            parts.append(serialize_attribute(attribute))

    parts.append("\n")

    return ''.join(parts)


def serialize_tag(tag):
    return "\n    #{}".format(
        serialize_symbol(tag.name),
    )


def serialize_attribute(attribute):
    return "\n    .{} ={}".format(
        serialize_identifier(attribute.id),
        indent(serialize_value(attribute.value))
    )


def serialize_value(pattern):
    multi = contain_new_line(pattern.elements)
    schema = "\n    {}" if multi else " {}"

    content = serialize_pattern(pattern)
    return schema.format(indent(content))


def serialize_pattern(pattern):
    return "".join([
        serialize_element(elem)
        for elem in pattern.elements
    ])


def serialize_element(element):
    if isinstance(element, ast.TextElement):
        return serialize_text_element(element)
    if isinstance(element, ast.Placeable):
        return serialize_placeable(element)
    raise Exception('Unknown element type: {}'.format(element.type))


def serialize_text_element(text):
    return text.value


def serialize_placeable(placeable):
    expr = placeable.expression

    if isinstance(expr, ast.Placeable):
        return "{{{}}}".format(
            serialize_placeable(expr))
    if isinstance(expr, ast.SelectExpression):
        return "{{{}}}".format(
            serialize_select_expression(expr))
    if isinstance(expr, ast.Expression):
        return "{{ {} }}".format(
            serialize_expression(expr))


def serialize_expression(expression):
    if isinstance(expression, ast.StringExpression):
        return serialize_string_expression(expression)
    if isinstance(expression, ast.NumberExpression):
        return serialize_number_expression(expression)
    if isinstance(expression, ast.MessageReference):
        return serialize_message_reference(expression)
    if isinstance(expression, ast.ExternalArgument):
        return serialize_external_argument(expression)
    if isinstance(expression, ast.AttributeExpression):
        return serialize_attribute_expression(expression)
    if isinstance(expression, ast.VariantExpression):
        return serialize_variant_expression(expression)
    if isinstance(expression, ast.CallExpression):
        return serialize_call_expression(expression)
    raise Exception('Unknown expression type: {}'.format(expression.type))


def serialize_string_expression(expr):
    return "\"{}\"".format(expr.value)


def serialize_number_expression(expr):
    return expr.value


def serialize_message_reference(expr):
    return serialize_identifier(expr.id)


def serialize_external_argument(expr):
    return "${}".format(serialize_identifier(expr.id))


def serialize_select_expression(expr):
    parts = []

    if expr.expression:
        selector = " {} ->".format(
            serialize_expression(expr.expression)
        )
        parts.append(selector)

    for variant in expr.variants:
        parts.append(serialize_variant(variant))

    parts.append("\n")

    return "".join(parts)


def serialize_variant(variant):
    return "\n{}[{}]{}".format(
        "   *" if variant.default else "    ",
        serialize_variant_key(variant.key),
        indent(serialize_value(variant.value))
    )


def serialize_attribute_expression(expr):
    return "{}.{}".format(
        serialize_identifier(expr.id),
        serialize_identifier(expr.name),
    )


def serialize_variant_expression(expr):
    return "{}[{}]".format(
        serialize_identifier(expr.id),
        serialize_variant_key(expr.key),
    )


def serialize_call_expression(expr):
    return "{}({})".format(
        serialize_function(expr.callee),
        ", ".join([
            serialize_call_argument(arg)
            for arg in expr.args
        ])
    )


def serialize_call_argument(arg):
    if isinstance(arg, ast.Expression):
        return serialize_expression(arg)
    if isinstance(arg, ast.NamedArgument):
        return serialize_named_argument(arg)


def serialize_named_argument(arg):
    return "{}: {}".format(
        serialize_identifier(arg.name),
        serialize_argument_value(arg.val)
    )


def serialize_argument_value(argval):
    if isinstance(argval, ast.StringExpression):
        return serialize_string_expression(argval)
    if isinstance(argval, ast.NumberExpression):
        return serialize_number_expression(argval)
    raise Exception('Unknown argument type: {}'.format(argval.type))


def serialize_identifier(identifier):
    return identifier.name


def serialize_symbol(symbol):
    return symbol.name


def serialize_variant_key(key):
    if isinstance(key, ast.Symbol):
        return serialize_symbol(key)
    if isinstance(key, ast.NumberExpression):
        return serialize_number_expression(key)
    raise Exception('Unknown variant key type: {}'.format(key.type))


def serialize_function(function):
    return function.name
