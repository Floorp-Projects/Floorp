# coding=utf8
from __future__ import unicode_literals

import os
import codecs
import logging

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
from .util import get_message


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

              helpers: LITERAL, EXTERNAL_ARGUMENT, MESSAGE_REFERENCE
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
                logger.warn(u'Syntax error in {}: {}'.format(path, msg))

        return ast

    def read_legacy_resource(self, path):
        """Read a legacy resource and parse it into a dict."""
        parser = getParser(path)
        parser.readFile(path)
        # Transform the parsed result which is an iterator into a dict.
        return {entity.key: entity.val for entity in parser}

    def add_reference(self, path, realpath=None):
        """Add an FTL AST to this context's reference resources."""
        fullpath = os.path.join(self.reference_dir, realpath or path)
        try:
            ast = self.read_ftl_resource(fullpath)
        except IOError as err:
            logger = logging.getLogger('migrate')
            logger.error(u'Missing reference file: {}'.format(path))
            raise err
        except UnicodeDecodeError as err:
            logger = logging.getLogger('migrate')
            logger.error(u'Error reading file {}: {}'.format(path, err))
            raise err
        else:
            self.reference_resources[path] = ast

    def add_localization(self, path):
        """Add an existing localization resource.

        If it's an FTL resource, add an FTL AST.  Otherwise, it's a legacy
        resource.  Use a compare-locales parser to create a dict of (key,
        string value) tuples.
        """
        fullpath = os.path.join(self.localization_dir, path)
        if fullpath.endswith('.ftl'):
            try:
                ast = self.read_ftl_resource(fullpath)
            except IOError:
                logger = logging.getLogger('migrate')
                logger.warn(u'Missing localization file: {}'.format(path))
            except UnicodeDecodeError as err:
                logger = logging.getLogger('migrate')
                logger.warn(u'Error reading file {}: {}'.format(path, err))
            else:
                self.localization_resources[path] = ast
        else:
            try:
                collection = self.read_legacy_resource(fullpath)
            except IOError:
                logger = logging.getLogger('migrate')
                logger.warn(u'Missing localization file: {}'.format(path))
            else:
                self.localization_resources[path] = collection

    def add_transforms(self, path, transforms):
        """Define transforms for path.

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

        for node in transforms:
            # Scan `node` for `Source` nodes and collect the information they
            # store into a set of dependencies.
            dependencies = fold(get_sources, node, set())
            # Set these sources as dependencies for the current transform.
            self.dependencies[(path, node.id.name)] = dependencies

        path_transforms = self.transforms.setdefault(path, [])
        path_transforms += transforms

    def get_source(self, path, key):
        """Get an entity value from the localized source.

        Used by the `Source` transform.
        """
        if path.endswith('.ftl'):
            resource = self.localization_resources[path]
            return get_message(resource.body, key)
        else:
            resource = self.localization_resources[path]
            return resource.get(key, None)

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
            # Merge all known legacy translations.
            changeset = {
                (path, key)
                for path, strings in self.localization_resources.iteritems()
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

            # If none of the transforms is in the given changeset, the merged
            # snapshot is identical to the current translation. We compare
            # JSON trees rather then use filtering by `in_changeset` to account
            # for translations removed from `reference`.
            if snapshot.to_json() == current.to_json():
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
