from __future__ import absolute_import


def to_chrome_path(path):
    return path.replace('/locales/en-US', '')
    return path


def get_dtd_path(name, dtds):
    return dtds[name[1:-1]]['file']


def get_entity_name(s):
    return s[1:-1]


def ind(n=0):
    return ' ' * 4 * n


def add_copy(dtd, entity_id, indent, prefix=""):
    result = '{0}{1}COPY(\n'.format(ind(indent), prefix)
    result += '{0}\'{1}\',\n'.format(ind(indent + 1), to_chrome_path(dtd))
    result += '{0}\'{1}\',\n'.format(
        ind(indent + 1), get_entity_name(entity_id))
    result += '{0}),\n'.format(ind(indent))
    return result


def build_migration(messages, dtds, data):
    res = 'import fluent.syntax as FTL\n'

    features = ['COPY']
    res += 'from fluent.migrate import {0}\n'.format(','.join(features))
    desc = 'Bug {0} - {1}, part {{index}}'.format(
        data['bug_id'], data['description'])
    res += '\n\ndef migrate(ctx):\n    """{0}"""\n\n'.format(desc)

    for dtd_path in data['dtd']:
        res += "{0}ctx.maybe_add_localization('{1}')\n".format(
            ind(1), to_chrome_path(dtd_path))

    res += '\n'
    res += ind(1) + 'ctx.add_transforms(\n'
    res += ind(2) + '{0},\n'.format(to_chrome_path(data['ftl']))
    res += ind(2) + '{0},\n'.format(to_chrome_path(data['ftl']))
    res += ind(2) + '[\n'
    for l10n_id in messages:
        msg = messages[l10n_id]
        res += ind(3) + 'FTL.Message(\n'
        res += ind(4) + 'id=FTL.Identifier(\'{0}\'),\n'.format(l10n_id)
        if msg['value']:
            res += add_copy(get_dtd_path(msg['value'], dtds), msg['value'], 4, 'value=')
        if msg['attrs']:
            res += ind(4) + 'attributes=[\n'
            for name in msg['attrs']:
                attr = msg['attrs'][name]
                res += ind(5) + 'FTL.Attribute(\n'
                res += ind(6) + 'FTL.Identifier(\'{0}\'),\n'.format(name)
                res += add_copy(get_dtd_path(attr, dtds), attr, 6)
                res += ind(5) + '),\n'
            res += ind(4) + '],\n'
        res += ind(3) + '),\n'
    res += ind(2) + ']\n'
    res += ind(1) + ')'

    return res
