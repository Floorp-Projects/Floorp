# coding=utf8
from __future__ import unicode_literals
from __future__ import absolute_import

import os
import codecs
from functools import partial
import logging
from six.moves import zip_longest

import fluent.syntax.ast as FTL
from fluent.syntax.parser import FluentParser
from fluent.syntax.serializer import FluentSerializer
from fluent.migrate.util import fold
from compare_locales.parser import getParser
from compare_locales.plurals import CATEGORIES_BY_LOCALE

from .transforms import Source
from .merge import merge_resource
from .util import get_message
from .errors import (
    EmptyLocalizationError,
    UnreadableReferenceError,
)


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

        - A list of `FTL.Message` or `FTL.Term` objects some of whose nodes
          are special helper or transform nodes:

              helpers: VARIABLE_REFERENCE, MESSAGE_REFERENCE, TERM_REFERENCE
              transforms: COPY, REPLACE_IN_TEXT, REPLACE, PLURALS, CONCAT
    """

    def __init__(self, lang, reference_dir, localization_dir):
        self.fluent_parser = FluentParser(with_spans=False)
        self.fluent_serializer = FluentSerializer()

        # An iterable of plural category names relevant to the context's
        # language.  E.g. ('one', 'other') for English.
        try:
            self.plural_categories = CATEGORIES_BY_LOCALE[lang]
        except KeyError as locale_key:
            logger = logging.getLogger('migrate')
            logger.warning(
                'Plural rule for "{}" is not defined in '
                'compare-locales'.format(locale_key))
            self.plural_categories = ('one', 'other')

        # Paths to directories with input data, relative to CWD.
        self.reference_dir = reference_dir
        self.localization_dir = localization_dir

        # Parsed input resources stored by resource path.
        self.reference_resources = {}
        self.localization_resources = {}
        self.target_resources = {}

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
            logger.warning('Unable to read file {}: {}'.format(path, err))
            raise err
        finally:
            f.close()

        ast = self.fluent_parser.parse(contents)

        annots = [
            annot
            for entry in ast.body
            if isinstance(entry, FTL.Junk)
            for annot in entry.annotations
        ]

        if len(annots):
            logger = logging.getLogger('migrate')
            for annot in annots:
                msg = annot.message
                logger.warning('Syntax error in {}: {}'.format(path, msg))

        return ast

    def read_legacy_resource(self, path):
        """Read a legacy resource and parse it into a dict."""
        parser = getParser(path)
        parser.readFile(path)
        # Transform the parsed result which is an iterator into a dict.
        return {entity.key: entity.val for entity in parser}

    def read_reference_ftl(self, path):
        """Read and parse a reference FTL file.

        A missing resource file is a fatal error and will raise an
        UnreadableReferenceError.
        """
        fullpath = os.path.join(self.reference_dir, path)
        try:
            return self.read_ftl_resource(fullpath)
        except IOError:
            error_message = 'Missing reference file: {}'.format(fullpath)
            logging.getLogger('migrate').error(error_message)
            raise UnreadableReferenceError(error_message)
        except UnicodeDecodeError as err:
            error_message = 'Error reading file {}: {}'.format(fullpath, err)
            logging.getLogger('migrate').error(error_message)
            raise UnreadableReferenceError(error_message)

    def read_localization_ftl(self, path):
        """Read and parse an existing localization FTL file.

        Create a new FTL.Resource if the file doesn't exist or can't be
        decoded.
        """
        fullpath = os.path.join(self.localization_dir, path)
        try:
            return self.read_ftl_resource(fullpath)
        except IOError:
            logger = logging.getLogger('migrate')
            logger.info(
                'Localization file {} does not exist and '
                'it will be created'.format(path))
            return FTL.Resource()
        except UnicodeDecodeError:
            logger = logging.getLogger('migrate')
            logger.warning(
                'Localization file {} has broken encoding. '
                'It will be re-created and some translations '
                'may be lost'.format(path))
            return FTL.Resource()

    def maybe_add_localization(self, path):
        """Add a localization resource to migrate translations from.

        Uses a compare-locales parser to create a dict of (key, string value)
        tuples.
        For Fluent sources, we store the AST.
        """
        try:
            fullpath = os.path.join(self.localization_dir, path)
            if not fullpath.endswith('.ftl'):
                collection = self.read_legacy_resource(fullpath)
            else:
                collection = self.read_ftl_resource(fullpath)
        except IOError:
            logger = logging.getLogger('migrate')
            logger.warning('Missing localization file: {}'.format(path))
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

        reference_ast = self.read_reference_ftl(reference)
        self.reference_resources[target] = reference_ast

        for node in transforms:
            ident = node.id.name
            # Scan `node` for `Source` nodes and collect the information they
            # store into a set of dependencies.
            dependencies = fold(get_sources, node, set())
            # Set these sources as dependencies for the current transform.
            self.dependencies[(target, ident)] = dependencies

            # The target Fluent message should exist in the reference file. If
            # it doesn't, it's probably a typo.
            if get_message(reference_ast.body, ident) is None:
                logger = logging.getLogger('migrate')
                logger.warning(
                    '{} "{}" was not found in {}'.format(
                        type(node).__name__, ident, reference))

        # Keep track of localization resource paths which were defined as
        # sources in the transforms.
        expected_paths = set()

        # Read all legacy translation files defined in Source transforms. This
        # may fail but a single missing legacy resource doesn't mean that the
        # migration can't succeed.
        for dependencies in self.dependencies.values():
            for path in set(path for path, _ in dependencies):
                expected_paths.add(path)
                self.maybe_add_localization(path)

        # However, if all legacy resources are missing, bail out early. There
        # are no translations to migrate. We'd also get errors in hg annotate.
        if len(expected_paths) > 0 and len(self.localization_resources) == 0:
            error_message = 'No localization files were found'
            logging.getLogger('migrate').error(error_message)
            raise EmptyLocalizationError(error_message)

        # Add the current transforms to any other transforms added earlier for
        # this path.
        path_transforms = self.transforms.setdefault(target, [])
        path_transforms += transforms

        if target not in self.target_resources:
            target_ast = self.read_localization_ftl(target)
            self.target_resources[target] = target_ast

    def get_legacy_source(self, path, key):
        """Get an entity value from a localized legacy source.

        Used by the `Source` transform.
        """
        resource = self.localization_resources[path]
        return resource.get(key, None)

    def get_fluent_source_pattern(self, path, key):
        """Get a pattern from a localized Fluent source.

        If the key contains a `.`, does an attribute lookup.
        Used by the `COPY_PATTERN` transform.
        """
        resource = self.localization_resources[path]
        msg_key, _, attr_key = key.partition('.')
        found = None
        for entry in resource.body:
            if isinstance(entry, (FTL.Message, FTL.Term)):
                if entry.id.name == msg_key:
                    found = entry
                    break
        if found is None:
            return None
        if not attr_key:
            return found.value
        for attribute in found.attributes:
            if attribute.id.name == attr_key:
                return attribute.value
        return None

    def messages_equal(self, res1, res2):
        """Compare messages and terms of two FTL resources.

        Uses FTL.BaseNode.equals to compare all messages/terms
        in two FTL resources.
        If the order or number of messages differ, the result is also False.
        """
        def message_id(message):
            "Return the message's identifer name for sorting purposes."
            return message.id.name

        messages1 = sorted(
            (entry for entry in res1.body
             if isinstance(entry, FTL.Message)
                or isinstance(entry, FTL.Term)),
            key=message_id)
        messages2 = sorted(
            (entry for entry in res2.body
             if isinstance(entry, FTL.Message)
                or isinstance(entry, FTL.Term)),
            key=message_id)
        for msg1, msg2 in zip_longest(messages1, messages2):
            if msg1 is None or msg2 is None:
                return False
            if not msg1.equals(msg2):
                return False
        return True

    def merge_changeset(self, changeset=None, known_translations=None):
        """Return a generator of FTL ASTs for the changeset.

        The input data must be configured earlier using the `add_*` methods.
        if given, `changeset` must be a set of (path, key) tuples describing
        which legacy translations are to be merged. If `changeset` is None,
        all legacy translations will be allowed to be migrated in a single
        changeset.

        We use the `in_changeset` method to determine if a message should be
        migrated for the given changeset.

        Given `changeset`, return a dict whose keys are resource paths and
        values are `FTL.Resource` instances.  The values will also be used to
        update this context's existing localization resources.
        """

        if changeset is None:
            # Merge all known legacy translations. Used in tests.
            changeset = {
                (path, key)
                for path, strings in self.localization_resources.items()
                if not path.endswith('.ftl')
                for key in strings.keys()
            }

        if known_translations is None:
            known_translations = changeset

        for path, reference in self.reference_resources.items():
            current = self.target_resources[path]
            transforms = self.transforms.get(path, [])
            in_changeset = partial(
                self.in_changeset, changeset, known_translations, path)

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
            self.target_resources[path] = snapshot

            # The result for this path is a complete `FTL.Resource`.
            yield path, snapshot

    def in_changeset(self, changeset, known_translations, path, ident):
        """Check if a message should be migrated in this changeset.

        The message is identified by path and ident.


        A message will be migrated only if all of its dependencies
        are present in the currently processed changeset.

        If a transform defined for this message points to a missing
        legacy translation, this message will not be merged. The
        missing legacy dependency won't be present in the changeset.

        This also means that partially translated messages (e.g.
        constructed from two legacy strings out of which only one is
        avaiable) will never be migrated.
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

        # Make sure all the dependencies are present in the current
        # changeset. Partial migrations are not currently supported.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=1321271
        # We only return True if our current changeset touches
        # the transform, and we have all of the dependencies.
        active_deps = message_deps & changeset
        available_deps = message_deps & known_translations
        return active_deps and message_deps == available_deps

    def serialize_changeset(self, changeset, known_translations=None):
        """Return a dict of serialized FTLs for the changeset.

        Given `changeset`, return a dict whose keys are resource paths and
        values are serialized FTL snapshots.
        """

        return {
            path: self.fluent_serializer.serialize(snapshot)
            for path, snapshot in self.merge_changeset(
                changeset, known_translations
            )
        }


logging.basicConfig()
