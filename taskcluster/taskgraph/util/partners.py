from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy
import json
import os

LOCALES_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))),
    'browser', 'locales', 'l10n-changesets.json'
)


def check_if_partners_enabled(config, tasks):
    if (
        config.params['release_enable_partners'] and
        config.kind.startswith('release-partner-repack')
    ) or (
        config.params['release_enable_emefree'] and
        config.kind.startswith('release-eme-free-repack')
    ):
        for task in tasks:
            yield task


def get_partner_config_by_kind(config, kind):
    """ Retrieve partner data starting from the manifest url, which points to a repository
    containing a default.xml that is intended to be drive the Google tool 'repo'. It
    descends into each partner repo to lookup and parse the repack.cfg file(s).

    Supports caching data by kind to avoid repeated requests, relying on the related kinds for
    partner repacking, signing, repackage, repackage signing all having the same kind prefix.
    """
    partner_subset = config.params['release_partners']
    partner_configs = config.params['release_partner_config'] or {}

    # TODO eme-free should be a partner; we shouldn't care about per-kind
    for k in partner_configs:
        if kind.startswith(k):
            kind_config = partner_configs[k]
            break
    else:
        return {}
    # if we're only interested in a subset of partners we remove the rest
    if isinstance(partner_subset, (list, tuple)):
        # TODO - should be fatal to have an unknown partner in partner_subset
        for partner in kind_config.keys():
            if partner not in partner_subset:
                del(kind_config[partner])

    return kind_config


def _fix_subpartner_locales(orig_config, all_locales):
    subpartner_config = deepcopy(orig_config)
    # Get an ordered list of subpartner locales that is a subset of all_locales
    subpartner_config['locales'] = sorted(list(
        set(orig_config['locales']) & set(all_locales)
    ))
    return subpartner_config


def fix_partner_config(orig_config):
    pc = {}
    with open(LOCALES_FILE, 'r') as fh:
        all_locales = json.load(fh).keys()
    # l10n-changesets.json doesn't include en-US, but the repack list does
    if 'en-US' not in all_locales:
        all_locales.append('en-US')
    for kind, kind_config in orig_config.iteritems():
        for partner, partner_config in kind_config.iteritems():
            for subpartner, subpartner_config in partner_config.iteritems():
                # get rid of empty subpartner configs
                if not subpartner_config:
                    continue
                # Make sure our locale list is a subset of all_locales
                pc.setdefault(kind, {}).setdefault(partner, {})[subpartner] = \
                    _fix_subpartner_locales(subpartner_config, all_locales)
    return pc


# seems likely this exists elsewhere already
def get_ftp_platform(platform):
    if platform.startswith('win32'):
        return 'win32'
    elif platform.startswith('win64'):
        return 'win64'
    elif platform.startswith('macosx'):
        return 'mac'
    elif platform.startswith('linux-'):
        return 'linux-i686'
    elif platform.startswith('linux64'):
        return 'linux-x86_64'
    else:
        raise ValueError('Unimplemented platform %s'.format(platform))


# Ugh
def locales_per_build_platform(build_platform, locales):
    if build_platform.startswith('mac'):
        exclude = ['ja']
    else:
        exclude = ['ja-JP-mac']
    return [locale for locale in locales if locale not in exclude]
