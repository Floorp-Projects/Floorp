from __future__ import absolute_import
from fluent.syntax import ast
from fluent.syntax.serializer import FluentSerializer


def get_value_from_dtd(name, dtd):
    return dtd[name[1:-1]]['value']


def build_ftl(messages, dtd, data):
    res = ast.Resource()

    for id_str in messages:
        msg = messages[id_str]
        l10n_id = ast.Identifier(id_str)
        val = None
        attrs = []
        if msg['value']:
            dtd_val = get_value_from_dtd(msg['value'], dtd)
            val = ast.Pattern([ast.TextElement(dtd_val)])
        for attr_name in msg['attrs']:
            dtd_val = get_value_from_dtd(msg['attrs'][attr_name], dtd)
            attr_val = ast.Pattern([ast.TextElement(dtd_val)])
            attrs.append(ast.Attribute(ast.Identifier(attr_name), attr_val))

        m = ast.Message(l10n_id, val, attrs)
        res.body.append(m)

    serializer = FluentSerializer()
    return serializer.serialize(res)
