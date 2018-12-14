# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Merge resources across channels.

Merging resources is done over a series of parsed resources, or source
strings.
The nomenclature is that the resources are ordered from newest to oldest.
The generated file structure is taken from the newest file, and then the
next-newest, etc. The values of the returned entities are taken from the
newest to the oldest resource, too.

In merge_resources, there's an option to choose the values from oldest
to newest instead.
'''

from collections import OrderedDict, defaultdict
from codecs import encode
import six


from compare_locales import parser as cl
from compare_locales.parser.base import StickyEntry
from compare_locales.compare.utils import AddRemove


class MergeNotSupportedError(ValueError):
    pass


def merge_channels(name, resources):
    try:
        parser = cl.getParser(name)
    except UserWarning:
        raise MergeNotSupportedError(
            'Unsupported file format ({}).'.format(name))

    entities = merge_resources(parser, resources)
    return encode(serialize_legacy_resource(entities), parser.encoding)


def merge_resources(parser, resources, keep_newest=True):
    '''Merge parsed or unparsed resources, returning a enumerable of Entities.

    Resources are ordered from newest to oldest in the input. The structure
    of the generated content is taken from the newest resource first, and
    then filled by the next etc.
    Values are also taken from the newest, unless keep_newest is False,
    then values are taken from the oldest first.
    '''

    def parse_resource(resource):
        # The counter dict keeps track of number of identical comments.
        counter = defaultdict(int)
        if isinstance(resource, bytes):
            parser.readContents(resource)
            resource = parser.walk()
        pairs = [get_key_value(entity, counter) for entity in resource]
        return OrderedDict(pairs)

    def get_key_value(entity, counter):
        if isinstance(entity, cl.Comment):
            counter[entity.val] += 1
            # Use the (value, index) tuple as the key. AddRemove will
            # de-deplicate identical comments at the same index.
            return ((entity.val, counter[entity.val]), entity)

        if isinstance(entity, cl.Whitespace):
            # Use the Whitespace instance as the key so that it's always
            # unique. Adjecent whitespace will be folded into the longer one in
            # prune.
            return (entity, entity)

        return (entity.key, entity)

    entities = six.moves.reduce(
        lambda x, y: merge_two(x, y, keep_newer=keep_newest),
        map(parse_resource, resources))
    return entities.values()


def merge_two(newer, older, keep_newer=True):
    '''Merge two OrderedDicts.

    The order of the result dict is determined by `newer`.
    The values in the dict are the newer ones by default, too.
    If `keep_newer` is False, the values will be taken from the older
    dict.
    '''
    diff = AddRemove()
    diff.set_left(newer.keys())
    diff.set_right(older.keys())

    # Create a flat sequence of all entities in order reported by AddRemove.
    get_entity = get_newer_entity if keep_newer else get_older_entity
    contents = [(key, get_entity(newer, older, key)) for _, key in diff]

    def prune(acc, cur):
        _, entity = cur
        if entity is None:
            # Prune Nones which stand for duplicated comments.
            return acc

        if len(acc) and isinstance(entity, cl.Whitespace):
            _, prev_entity = acc[-1]

            if isinstance(prev_entity, cl.Whitespace):
                # Prefer the longer whitespace.
                if len(entity.all) > len(prev_entity.all):
                    acc[-1] = (entity, entity)
                return acc

        acc.append(cur)
        return acc

    pruned = six.moves.reduce(prune, contents, [])
    return OrderedDict(pruned)


def get_newer_entity(newer, older, key):
    entity = newer.get(key, None)

    # Always prefer the newer version.
    if entity is not None:
        return entity

    return older.get(key)


def get_older_entity(newer, older, key):
    entity = older.get(key, None)

    # If we don't have an older version, or it's a StickyEntry,
    # get a newer version
    if entity is None or isinstance(entity, StickyEntry):
        return newer.get(key)

    return entity


def serialize_legacy_resource(entities):
    return "".join((entity.all for entity in entities))
