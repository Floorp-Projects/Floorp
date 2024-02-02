from __future__ import annotations
from typing import Dict, Optional, Set, Tuple, cast

import os
import codecs
from functools import partial
import logging
from itertools import zip_longest

from compare_locales.parser import getParser
from compare_locales.plurals import get_plural
import fluent.syntax.ast as FTL
from fluent.syntax.parser import FluentParser
from fluent.syntax.serializer import FluentSerializer

from .changesets import Changes
from .errors import UnreadableReferenceError
from .evaluator import Evaluator
from .merge import merge_resource
from .transforms import Source


class InternalContext:
    """Internal context for merging translation resources.

    For the public interface, see `context.MigrationContext`.
    """

    dependencies: Dict[Tuple[str, str], Set[Tuple[str, Source]]] = {}
    localization_dir: str
    reference_dir: str

    def __init__(self, lang, enforce_translated=False):
        self.fluent_parser = FluentParser(with_spans=False)
        self.fluent_serializer = FluentSerializer()

        # An iterable of plural category names relevant to the context's
        # language.  E.g. ('one', 'other') for English.
        self.plural_categories = get_plural(lang)
        if self.plural_categories is None:
            logger = logging.getLogger("migrate")
            logger.warning(
                f'Plural rule for "{lang}" is not defined in "compare-locales"'
            )
            self.plural_categories = ("one", "other")

        self.enforce_translated = enforce_translated
        # Parsed input resources stored by resource path.
        self.reference_resources = {}
        self.localization_resources = {}
        self.target_resources = {}

        # An iterable of `FTL.Message` objects some of whose nodes can be the
        # transform operations.
        self.transforms = {}

        # The evaluator instance is an AST transformer capable of walking an
        # AST hierarchy and evaluating nodes which are migration Transforms.
        self.evaluator = Evaluator(self)

    def read_ftl_resource(self, path: str):
        """Read an FTL resource and parse it into an AST."""
        f = codecs.open(path, "r", "utf8")
        try:
            contents = f.read()
        except UnicodeDecodeError as err:
            logger = logging.getLogger("migrate")
            logger.warning(f"Unable to read file {path}: {err}")
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
            logger = logging.getLogger("migrate")
            for annot in annots:
                msg = annot.message
                logger.warning(f"Syntax error in {path}: {msg}")

        return ast

    def read_legacy_resource(self, path: str):
        """Read a legacy resource and parse it into a dict."""
        parser = getParser(path)
        parser.readFile(path)
        # Transform the parsed result which is an iterator into a dict.
        return {
            entity.key: entity.val
            for entity in parser
            if entity.localized or self.enforce_translated
        }

    def read_reference_ftl(self, path: str):
        """Read and parse a reference FTL file.

        A missing resource file is a fatal error and will raise an
        UnreadableReferenceError.
        """
        fullpath = os.path.join(self.reference_dir, path)
        try:
            return self.read_ftl_resource(fullpath)
        except OSError:
            error_message = f"Missing reference file: {fullpath}"
            logging.getLogger("migrate").error(error_message)
            raise UnreadableReferenceError(error_message)
        except UnicodeDecodeError as err:
            error_message = f"Error reading file {fullpath}: {err}"
            logging.getLogger("migrate").error(error_message)
            raise UnreadableReferenceError(error_message)

    def read_localization_ftl(self, path: str):
        """Read and parse an existing localization FTL file.

        Create a new FTL.Resource if the file doesn't exist or can't be
        decoded.
        """
        fullpath = os.path.join(self.localization_dir, path)
        try:
            return self.read_ftl_resource(fullpath)
        except OSError:
            logger = logging.getLogger("migrate")
            logger.info(
                "Localization file {} does not exist and "
                "it will be created".format(path)
            )
            return FTL.Resource()
        except UnicodeDecodeError:
            logger = logging.getLogger("migrate")
            logger.warning(
                "Localization file {} has broken encoding. "
                "It will be re-created and some translations "
                "may be lost".format(path)
            )
            return FTL.Resource()

    def maybe_add_localization(self, path: str):
        """Add a localization resource to migrate translations from.

        Uses a compare-locales parser to create a dict of (key, string value)
        tuples.
        For Fluent sources, we store the AST.
        """
        try:
            fullpath = os.path.join(self.localization_dir, path)
            if not fullpath.endswith(".ftl"):
                collection = self.read_legacy_resource(fullpath)
            else:
                collection = self.read_ftl_resource(fullpath)
        except OSError:
            logger = logging.getLogger("migrate")
            logger.warning(f"Missing localization file: {path}")
        else:
            self.localization_resources[path] = collection

    def get_legacy_source(self, path: str, key: str):
        """Get an entity value from a localized legacy source.

        Used by the `Source` transform.
        """
        resource = self.localization_resources[path]
        return resource.get(key, None)

    def get_fluent_source_pattern(self, path: str, key: str):
        """Get a pattern from a localized Fluent source.

        If the key contains a `.`, does an attribute lookup.
        Used by the `COPY_PATTERN` transform.
        """
        resource = self.localization_resources[path]
        msg_key, _, attr_key = key.partition(".")
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
            (
                entry
                for entry in res1.body
                if isinstance(entry, FTL.Message) or isinstance(entry, FTL.Term)
            ),
            key=message_id,
        )
        messages2 = sorted(
            (
                entry
                for entry in res2.body
                if isinstance(entry, FTL.Message) or isinstance(entry, FTL.Term)
            ),
            key=message_id,
        )
        for msg1, msg2 in zip_longest(messages1, messages2):
            if msg1 is None or msg2 is None:
                return False
            if not msg1.equals(msg2):
                return False
        return True

    def merge_changeset(
        self,
        changeset: Optional[Changes] = None,
        known_translations: Optional[Changes] = None,
    ):
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
                if not path.endswith(".ftl")
                for key in strings.keys()
            }

        if known_translations is None:
            known_translations = changeset

        for path, reference in self.reference_resources.items():
            current = self.target_resources[path]
            transforms = self.transforms.get(path, [])
            in_changeset = partial(
                self.in_changeset, changeset, known_translations, path
            )

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

    def in_changeset(
        self, changeset: Changes, known_translations: Changes, path: str, ident
    ) -> bool:
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
        active_deps = cast(bool, message_deps & changeset)
        available_deps = message_deps & known_translations
        return active_deps and message_deps == available_deps

    def serialize_changeset(
        self, changeset: Changes, known_translations: Optional[Changes] = None
    ):
        """Return a dict of serialized FTLs for the changeset.

        Given `changeset`, return a dict whose keys are resource paths and
        values are serialized FTL snapshots.
        """

        return {
            path: self.fluent_serializer.serialize(snapshot)
            for path, snapshot in self.merge_changeset(changeset, known_translations)
        }

    def evaluate(self, node):
        return self.evaluator.visit(node)


logging.basicConfig()
