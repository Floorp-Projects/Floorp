# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Do transforms specific to l10n kind
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy

from mozbuild.chunkify import chunkify
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import split_symbol, join_symbol

transforms = TransformSequence()


def _parse_locales_file(locales_file, platform=None):
    """ Parse the passed locales file for a list of locales.
        If platform is unset matches all platforms.
    """
    locales = []
    if locales_file.endswith('json'):
        # Release process uses .json for locale files sometimes.
        raise NotImplementedError("Don't know how to parse a .json locales file")
    else:
        with open(locales_file, mode='r') as lf:
            locales = lf.read().split()
    return locales


@transforms.add
def all_locales_attribute(config, jobs):
    for job in jobs:
        locales = set(_parse_locales_file(job["locales-file"]))
        # ja-JP-mac is a mac-only locale, but there are no
        # mac builds being repacked, so just omit it unconditionally
        locales = locales - set(("ja-JP-mac", ))
        # Convert to mutable list.
        locales = list(sorted(locales))
        attributes = job.setdefault('attributes', {})
        attributes["all_locales"] = locales

        del job["locales-file"]
        yield job


@transforms.add
def chunk_locales(config, jobs):
    """ Utilizes chunking for l10n stuff """
    for job in jobs:
        chunks = job.get('chunks')
        if 'chunks' in job:
            del job['chunks']
        all_locales = job['attributes']['all_locales']
        if chunks:
            for this_chunk in range(1, chunks + 1):
                chunked = copy.deepcopy(job)
                chunked['name'] = chunked['name'].replace(
                    '/', '-{}/'.format(this_chunk), 1
                )
                chunked['run']['options'] = chunked['run'].get('options', [])
                my_locales = []
                my_locales = chunkify(all_locales, this_chunk, chunks)
                chunked['run']['options'].extend([
                    "locale={}".format(locale) for locale in my_locales
                    ])
                chunked['attributes']['l10n_chunk'] = str(this_chunk)
                chunked['attributes']['chunk_locales'] = my_locales

                # add the chunk number to the TH symbol
                group, symbol = split_symbol(
                    chunked.get('treeherder', {}).get('symbol', ''))
                symbol += str(this_chunk)
                chunked['treeherder']['symbol'] = join_symbol(group, symbol)
                yield chunked
        else:
            job['run']['options'] = job['run'].get('options', [])
            job['run']['options'].extend([
                "locale={}".format(locale) for locale in all_locales
                ])
            yield job


@transforms.add
def mh_config_replace_project(config, jobs):
    """ Replaces {project} in mh config entries with the current project """
    # XXXCallek This is a bad pattern but exists to satisfy ease-of-porting for buildbot
    for job in jobs:
        if not job['run'].get('using') == 'mozharness':
            # Nothing to do, not mozharness
            yield job
            continue
        job['run']['config'] = map(
            lambda x: x.format(project=config.params['project']),
            job['run']['config']
            )
        yield job


@transforms.add
def mh_options_replace_project(config, jobs):
    """ Replaces {project} in mh option entries with the current project """
    # XXXCallek This is a bad pattern but exists to satisfy ease-of-porting for buildbot
    for job in jobs:
        if not job['run'].get('using') == 'mozharness':
            # Nothing to do, not mozharness
            yield job
            continue
        job['run']['options'] = map(
            lambda x: x.format(project=config.params['project']),
            job['run']['options']
            )
        yield job
