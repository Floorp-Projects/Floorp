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


def add_copy(dtd, entity_id):
    result = '{ ' + 'COPY("{0}", "{1}")'.format(dtd, entity_id)+' }\n'
    return result


def make_header():
    res = '# coding=utf8\n\n'
    res += '# Any copyright is dedicated to the Public Domain.\n'
    res += '# http://creativecommons.org/publicdomain/zero/1.0/\n\n'
    res += 'from __future__ import absolute_import\n'
    res += 'import fluent.syntax.ast as FTL\n'
    res += 'from fluent.migrate.helpers import transforms_from\n'
    res += 'from fluent.migrate.helpers import MESSAGE_REFERENCE, '
    res += 'TERM_REFERENCE, VARIABLE_REFERENCE\n'
    res += 'from fluent.migrate import COPY, CONCAT, REPLACE\n'

    return res


def build_migration(messages, dtds, data):
    res = make_header()
    desc = 'Bug {0} - {1}, part {{index}}'.format(
        data['bug_id'], data['description'])
    res += '\n\ndef migrate(ctx):\n    """{0}"""\n\n'.format(desc)

    for dtd_path in data['dtd']:
        res += "{0}ctx.maybe_add_localization('{1}')\n".format(
            ind(1), to_chrome_path(dtd_path))

    res += '\n'
    res += ind(1) + 'ctx.add_transforms(\n'
    res += ind(2) + "'{0}',\n".format(to_chrome_path(data['ftl']))
    res += ind(2) + "'{0}',\n".format(to_chrome_path(data['ftl']))
    res += ind(2) + 'transforms_from(\n'
    res += '"""\n'
    for l10n_id in messages:
        msg = messages[l10n_id]
        if not msg['attrs']:
            entity = get_entity_name(msg['value'])
            entity_path = to_chrome_path(get_dtd_path(msg['value'], dtds))
            res += '{0} = {1}'.format(l10n_id, add_copy(entity_path, entity))
        else:
            res += '{0} = \n'.format(l10n_id)
            for name in msg['attrs']:
                attr = msg['attrs'][name]
                attr_name = get_entity_name(attr)
                entity_path = to_chrome_path(get_dtd_path(attr, dtds))
                res += '{0}.{1} = {2}'.format(ind(1), name, add_copy(entity_path, attr_name))
    res += '"""\n'
    res += ind(2) + ')\n'
    res += ind(1) + ')'

    return res
