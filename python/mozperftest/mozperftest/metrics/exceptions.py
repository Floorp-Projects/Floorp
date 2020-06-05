# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class MetricsMultipleTransformsError(Exception):
    """Raised when more than one transformer was specified.

    This is because intermediate results with the same data
    name are merged when being processed.
    """

    pass


class MetricsMissingResultsError(Exception):
    """Raised when no results could be found after parsing the intermediate results."""

    pass


class PerfherderValidDataError(Exception):
    """Raised when no valid data (int/float) can be found to build perfherder blob."""

    pass


class NotebookInvalidTransformError(Exception):
    """Raised when an invalid custom transformer is set."""

    pass


class NotebookTransformOptionsError(Exception):
    """Raised when an invalid option is given to a transformer."""

    pass


class NotebookTransformError(Exception):
    """Raised on generic errors within the transformers."""


class NotebookDuplicateTransformsError(Exception):
    """Raised when a directory contains more than one transformers have the same class name."""

    pass


class NotebookInvalidPathError(Exception):
    """Raised when an invalid path is given."""

    pass
