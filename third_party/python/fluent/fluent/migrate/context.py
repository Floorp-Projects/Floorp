# coding=utf8
from __future__ import unicode_literals

import os
import codecs
import logging

try:
    from itertools import zip_longest
except ImportError:
    from itertools import izip_longest as zip_longest

import fluent.syntax.ast as FTL
from fluent.syntax.parser import FluentParser
from fluent.syntax.serializer import FluentSerializer
from fluent.util import fold
try:
    from compare_locales.parser import getParser
except ImportError:
    def getParser(path):
        raise RuntimeError('compare-locales required')

from .cldr import get_plural_categories
from .transforms import Source
from .merge import merge_resource
from .errors import NotSupportedError, UnreadableReferenceError


class MergeContext(object):
    """Stateful context for merging translation resources.

    `MergeContext` must be configured with the target language and the
    directory locations of the input data.

    The transformation takes four types of input data:

        - The en-US FTL reference files which will be used as templates for
          message order, comments and sections.

        - The current FTL files for the given language.

        - The legacy (DTD, properties) translation files for the given
          language.  The translations from these files will be transformed
          into FTL and merged into the existing FTL files for this language.

        - A list of `FTL.Message` objects some of whose nodes are special
          helper or transform nodes:

              helpers: EXTERNAL_ARGUMENT, MESSAGE_REFERENCE
              transforms: COPY, REPLACE_IN_TEXT, REPLACE, PLURALS, CONCAT
    """

    def __init__(self, lang, reference_dir, localization_dir):
        self.fluent_parser = FluentParser(with_spans=False)
        self.fluent_serializer = FluentSerializer()

        # An iterable of plural category names relevant to the context's
        # language.  E.g. ('one', 'other') for English.
        try:
            self.plural_categories = get_plural_categories(lang)
        except RuntimeError as e:
            print(e.message)
            self.plural_categories = 'en'

        # Paths to directories with input data, relative to CWD.
        self.reference_dir = reference_dir
        self.localization_dir = localization_dir

        # Parsed input resources stored by resource path.
        self.reference_resources = {}
        self.localization_resources = {}

        # An iterable of `FTL.Message` objects some of whose nodes can be the
        # transform operations.
        self.transforms = {}

        # A dict whose keys are `(path, key)` tuples corresponding to target
        # FTL translations, and values are sets of `(path, key)` tuples
        # corresponding to localized entities which will be migrated.
        self.dependencies = {}

    def read_ftl_resource(self, path):
        """Read an FTL resource and parse it into an AST."""
        f = codecs.open(path, 'r', 'utf8')
        try:
            contents = f.read()
        except UnicodeDecodeError as err:
            logger = logging.getLogger('migrate')
            logger.warn('Unable to read file {}: {}'.format(path, err))
            raise err
        finally:
            f.close()

        ast = self.fluent_parser.parse(contents)

        annots = [
            annot
            for entry in ast.body
            for annot in entry.annotations
        ]

        if len(annots):
            logger = logging.getLogger('migrate')
            for annot in annots:
                msg = annot.message
                logger.warn('Syntax error in {}: {}'.format(path, msg))

        return ast

    def read_legacy_resource(self, path):
        """Read a legacy resource and parse it into a dict."""
        parser = getParser(path)
        parser.readFile(path)
        # Transform the parsed result which is an iterator into a dict.
        return {entity.key: entity.val for entity in parser}

    def maybe_add_localization(self, path):
        """Add a localization resource to migrate translations from.

        Only legacy resources can be added as migration sources.  The resource
        may be missing on disk.

        Uses a compare-locales parser to create a dict of (key, string value)
        tuples.
        """
        if path.endswith('.ftl'):
            error_message = (
                'Migrating translations from Fluent files is not supported '
                '({})'.format(path))
            logging.getLogger('migrate').error(error_message)
            raise NotSupportedError(error_message)

        try:
            fullpath = os.path.join(self.localization_dir, path)
            collection = self.read_legacy_resource(fullpath)
        except IOError:
            logger = logging.getLogger('migrate')
            logger.warn('Missing localization file: {}'.format(path))
        else:
            self.localization_resources[path] = collection

    def add_transforms(self, target, reference, transforms):
        """Define transforms for target using reference as template.

        `target` is a path of the destination FTL file relative to the
        localization directory. `reference` is a path to the template FTL
        file relative to the reference directory.

        Each transform is an extended FTL node with `Transform` nodes as some
        values.  Transforms are stored in their lazy AST form until
        `merge_changeset` is called, at which point they are evaluated to real
        FTL nodes with migrated translations.

        Each transform is scanned for `Source` nodes which will be used to
        build the list of dependencies for the transformed message.
        """
        def get_sources(acc, cur):
            if isinstance(cur, Source):
                acc.add((cur.path, cur.key))
            return acc

        refpath = os.path.join(self.reference_dir, reference)
        try:
            ast = self.read_ftl_resource(refpath)
        except IOError as err:
            error_message = 'Missing reference file: {}'.format(refpath)
            logging.getLogger('migrate').error(error_message)
            raise UnreadableReferenceError(error_message)
        except UnicodeDecodeError as err:
            error_message = 'Error reading file {}: {}'.format(refpath, err)
            logging.getLogger('migrate').error(error_message)
            raise UnreadableReferenceError(error_message)
        else:
            # The reference file will be used by the merge function as
            # a template for serializing the merge results.
            self.reference_resources[target] = ast

        for node in transforms:
            # Scan `node` for `Source` nodes and collect the information they
            # store into a set of dependencies.
            dependencies = fold(get_sources, node, set())
            # Set these sources as dependencies for the current transform.
            self.dependencies[(target, node.id.name)] = dependencies

            # Read all legacy translation files defined in Source transforms.
            for path in set(path for path, _ in dependencies):
                self.maybe_add_localization(path)

        path_transforms = self.transforms.setdefault(target, [])
        path_transforms += transforms

        if target not in self.localization_resources:
            fullpath = os.path.join(self.localization_dir, target)
            try:
                ast = self.read_ftl_resource(fullpath)
            except IOError:
                logger = logging.getLogger('migrate')
                logger.info(
                    'Localization file {} does not exist and '
                    'it will be created'.format(target))
            except UnicodeDecodeError:
                logger = logging.getLogger('migrate')
                logger.warn(
                    'Localization file {} will be re-created and some '
                    'translations might be lost'.format(target))
            else:
                self.localization_resources[target] = ast

    def get_source(self, path, key):
        """Get an entity value from a localized legacy source.

        Used by the `Source` transform.
        """
        resource = self.localization_resources[path]
        return resource.get(key, None)

    def messages_equal(self, res1, res2):
        """Compare messages of two FTL resources.

        Uses FTL.BaseNode.equals to compare all messages in two FTL resources.
        If the order or number of messages differ, the result is also False.
        """
        def message_id(message):
            "Return the message's identifer name for sorting purposes."
            return message.id.name

        messages1 = sorted(
            (entry for entry in res1.body if isinstance(entry, FTL.Message)),
            key=message_id)
        messages2 = sorted(
            (entry for entry in res2.body if isinstance(entry, FTL.Message)),
            key=message_id)
        for msg1, msg2 in zip_longest(messages1, messages2):
            if msg1 is None or msg2 is None:
                return False
            if not msg1.equals(msg2):
                return False
        return True

    def merge_changeset(self, changeset=None):
        """Return a generator of FTL ASTs for the changeset.

        The input data must be configured earlier using the `add_*` methods.
        if given, `changeset` must be a set of (path, key) tuples describing
        which legacy translations are to be merged.

        Given `changeset`, return a dict whose keys are resource paths and
        values are `FTL.Resource` instances.  The values will also be used to
        update this context's existing localization resources.
        """

        if changeset is None:
            # Merge all known legacy translations. Used in tests.
            changeset = {
                (path, key)
                for path, strings in self.localization_resources.iteritems()
                if not path.endswith('.ftl')
                for key in strings.iterkeys()
            }

        for path, reference in self.reference_resources.iteritems():
            current = self.localization_resources.get(path, FTL.Resource())
            transforms = self.transforms.get(path, [])

            def in_changeset(ident):
                """Check if entity should be merged.

                If at least one dependency of the entity is in the current
                set of changeset, merge it.
                """
                message_deps = self.dependencies.get((path, ident), None)

                # Don't merge if we don't have a transform for this message.
                if message_deps is None:
                    return False

                # As a special case, if a transform exists but has no
                # dependecies, it's a hardcoded `FTL.Node` which doesn't
                # migrate any existing translation but rather creates a new
                # one.  Merge it.
                if len(message_deps) == 0:
                    return True

                # If the intersection of the dependencies and the current
                # changeset is non-empty, merge this message.
                return message_deps & changeset

            # Merge legacy translations with the existing ones using the
            # reference as a template.
            snapshot = merge_resource(
                self, reference, current, transforms, in_changeset
            )

            # Skip this path if the messages in the merged snapshot are
            # identical to those in the current state of the localization file.
            # This may happen when:
            #
            #   - none of the transforms is in the changset, or
            #   - all messages which would be migrated by the context's
            #     transforms already exist in the current state.
            if self.messages_equal(current, snapshot):
                continue

            # Store the merged snapshot on the context so that the next merge
            # already takes it into account as the existing localization.
            self.localization_resources[path] = snapshot

            # The result for this path is a complete `FTL.Resource`.
            yield path, snapshot

    def serialize_changeset(self, changeset):
        """Return a dict of serialized FTLs for the changeset.

        Given `changeset`, return a dict whose keys are resource paths and
        values are serialized FTL snapshots.
        """

        return {
            path: self.fluent_serializer.serialize(snapshot)
            for path, snapshot in self.merge_changeset(changeset)
        }


logging.basicConfig()
