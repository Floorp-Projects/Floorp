import logging

import fluent.syntax.ast as FTL
from fluent.migrate.util import fold

from .transforms import Source
from .util import get_message, skeleton
from .errors import (
    EmptyLocalizationError,
    UnreadableReferenceError,
)
from ._context import InternalContext


__all__ = [
    'EmptyLocalizationError',
    'UnreadableReferenceError',
    'MigrationContext',
]


class MigrationContext(InternalContext):
    """Stateful context for merging translation resources.

    `MigrationContext` must be configured with the target locale and the
    directory locations of the input data.

    The transformation takes four types of input data:

        - The en-US FTL reference files which will be used as templates for
          message order, comments and sections. If the reference_dir is None,
          the migration will create Messages and Terms in the order given by
          the transforms.

        - The current FTL files for the given locale.

        - A list of `FTL.Message` or `FTL.Term` objects some of whose nodes
          are special helper or transform nodes:

              helpers: VARIABLE_REFERENCE, MESSAGE_REFERENCE, TERM_REFERENCE
              transforms: COPY, REPLACE_IN_TEXT, REPLACE, PLURALS, CONCAT
              fluent value helper: COPY_PATTERN

    The legacy (DTD, properties) translation files are deduced by the
    dependencies in the transforms. The translations from these files will be
    read from the localization_dir and transformed into FTL and merged
    into the existing FTL files for the given language.
    """

    def __init__(
        self, locale, reference_dir, localization_dir, enforce_translated=False
    ):
        super().__init__(
            locale, reference_dir, localization_dir,
            enforce_translated=enforce_translated
        )
        self.locale = locale
        # Paths to directories with input data, relative to CWD.
        self.reference_dir = reference_dir
        self.localization_dir = localization_dir

        # A dict whose keys are `(path, key)` tuples corresponding to target
        # FTL translations, and values are sets of `(path, key)` tuples
        # corresponding to localized entities which will be migrated.
        self.dependencies = {}

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

        For transforms that merely copy legacy messages or Fluent patterns,
        using `fluent.migrate.helpers.transforms_from` is recommended.
        """
        def get_sources(acc, cur):
            if isinstance(cur, Source):
                acc.add((cur.path, cur.key))
            return acc

        if self.reference_dir is None:
            # Add skeletons to resource body for each transform
            # if there's no reference.
            reference_ast = self.reference_resources.get(target)
            if reference_ast is None:
                reference_ast = FTL.Resource()
            reference_ast.body.extend(
                skeleton(transform) for transform in transforms
            )
        else:
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
            # Of course, only if we're having a reference.
            if self.reference_dir is None:
                continue
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
            for path in {path for path, _ in dependencies}:
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
