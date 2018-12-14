# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Serialize string changes.

The serialization logic is based on the cross-channel merge algorithm.
It's taking the file structure for the first file, and localizable entries
from the last.
Input data is the parsed reference as a list of parser.walk(),
the existing localized file, also a list of parser.walk(), and a dictionary
of newly added keys and raw values.
To remove a string from a localization, pass `None` as value for a key.

The marshalling between raw values and entities is done via Entity.unwrap
and Entity.wrap.

To avoid adding English reference strings into the generated file, the
actual entities in the reference are replaced with Placeholders, which
are removed in a final pass over the result of merge_resources. After that,
we also prune whitespace once more.`
'''

from codecs import encode
import six

from compare_locales.merge import merge_resources, serialize_legacy_resource
from compare_locales.parser import getParser
from compare_locales.parser.base import (
    Entity,
    PlaceholderEntity,
    Junk,
    Whitespace,
)


class SerializationNotSupportedError(ValueError):
    pass


def serialize(filename, reference, old_l10n, new_data):
    '''Returns a byte string of the serialized content to use.

    Input are a filename to create the right parser, a reference and
    an existing localization, both as the result of parser.walk().
    Finally, new_data is a dictionary of key to raw values to serialize.

    Raises a SerializationNotSupportedError if we don't support the file
    format.
    '''
    try:
        parser = getParser(filename)
    except UserWarning:
        raise SerializationNotSupportedError(
            'Unsupported file format ({}).'.format(filename))
    # create template, whitespace and all
    placeholders = [
        placeholder(entry)
        for entry in reference
        if not isinstance(entry, Junk)
    ]
    ref_mapping = {
        entry.key: entry
        for entry in reference
        if isinstance(entry, Entity)
    }
    # strip obsolete strings
    old_l10n = sanitize_old(ref_mapping.keys(), old_l10n, new_data)
    # create new Entities
    # .val can just be "", merge_channels doesn't need that
    new_l10n = []
    for key, new_raw_val in six.iteritems(new_data):
        if new_raw_val is None or key not in ref_mapping:
            continue
        ref_ent = ref_mapping[key]
        new_l10n.append(ref_ent.wrap(new_raw_val))

    merged = merge_resources(
        parser,
        [placeholders, old_l10n, new_l10n],
        keep_newest=False
    )
    pruned = prune_placeholders(merged)
    return encode(serialize_legacy_resource(pruned), parser.encoding)


def sanitize_old(known_keys, old_l10n, new_data):
    """Strip Junk and replace obsolete messages with placeholders.
    If new_data has `None` as a value, strip the existing translation.
    Use placeholders generously, so that we can rely on `prune_placeholders`
    to find their associated comments and remove them, too.
    """

    def should_placeholder(entry):
        # If entry is an Entity, check if it's obsolete
        # or marked to be removed.
        if not isinstance(entry, Entity):
            return False
        if entry.key not in known_keys:
            return True
        return entry.key in new_data and new_data[entry.key] is None

    return [
        placeholder(entry)
        if should_placeholder(entry)
        else entry
        for entry in old_l10n
        if not isinstance(entry, Junk)
    ]


def placeholder(entry):
    if isinstance(entry, Entity):
        return PlaceholderEntity(entry.key)
    return entry


def prune_placeholders(entries):
    pruned = [
        entry for entry in entries
        if not isinstance(entry, PlaceholderEntity)
    ]

    def prune_whitespace(acc, entity):
        if len(acc) and isinstance(entity, Whitespace):
            prev_entity = acc[-1]

            if isinstance(prev_entity, Whitespace):
                # Prefer the longer whitespace.
                if len(entity.all) > len(prev_entity.all):
                    acc[-1] = entity
                return acc

        acc.append(entity)
        return acc

    return six.moves.reduce(prune_whitespace, pruned, [])
