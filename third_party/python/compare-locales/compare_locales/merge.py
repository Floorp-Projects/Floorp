# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Merge resources across channels.'

from collections import OrderedDict, defaultdict
from codecs import encode


from compare_locales import parser as cl
from compare_locales.compare import AddRemove


class MergeNotSupportedError(ValueError):
    pass


def merge_channels(name, *resources):
    try:
        parser = cl.getParser(name)
    except UserWarning:
        raise MergeNotSupportedError(
            'Unsupported file format ({}).'.format(name))

    # A map of comments to the keys of entities they belong to.
    comments = {}

    def parse_resource(resource):
        # The counter dict keeps track of number of identical comments.
        counter = defaultdict(int)
        parser.readContents(resource)
        pairs = [get_key_value(entity, counter) for entity in parser.walk()]
        return OrderedDict(pairs)

    def get_key_value(entity, counter):
        if isinstance(entity, cl.Comment):
            counter[entity.all] += 1
            # Use the (value, index) tuple as the key. AddRemove will
            # de-deplicate identical comments at the same index.
            return ((entity.all, counter[entity.all]), entity)

        if isinstance(entity, cl.Whitespace):
            # Use the Whitespace instance as the key so that it's always
            # unique. Adjecent whitespace will be folded into the longer one in
            # prune.
            return (entity, entity)

        # When comments change, AddRemove gives us one 'add' and one 'delete'
        # (because a comment's key is its content).  In merge_two we'll try to
        # de-duplicate comments by looking at the entity they belong to.  Set
        # up the back-reference from the comment to its entity here.
        if isinstance(entity, cl.Entity) and entity.pre_comment:
            comments[entity.pre_comment] = entity.key

        return (entity.key, entity)

    entities = reduce(
        lambda x, y: merge_two(comments, x, y),
        map(parse_resource, resources))

    return encode(serialize_legacy_resource(entities), parser.encoding)


def merge_two(comments, newer, older):
    diff = AddRemove()
    diff.set_left(newer.keys())
    diff.set_right(older.keys())

    def get_entity(key):
        entity = newer.get(key, None)

        # Always prefer the newer version.
        if entity is not None:
            return entity

        entity = older.get(key)

        # If it's an old comment attached to an entity, try to find that
        # entity in newer and return None to use its comment instead in prune.
        if isinstance(entity, cl.Comment) and entity in comments:
            next_entity = newer.get(comments[entity], None)
            if next_entity is not None and next_entity.pre_comment:
                # We'll prune this before returning the merged result.
                return None

        return entity

    # Create a flat sequence of all entities in order reported by AddRemove.
    contents = [(key, get_entity(key)) for _, key in diff]

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

    pruned = reduce(prune, contents, [])
    return OrderedDict(pruned)


def serialize_legacy_resource(entities):
    return "".join((entity.all for entity in entities.values()))
