# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mach.decorators import Command, CommandArgument


@Command(
    "data-review",
    category="misc",
    description="Generate a skeleton data review request form for a given bug's data",
)
@CommandArgument(
    "bug", default=None, nargs="?", type=str, help="bug number or search pattern"
)
def data_review(command_context, bug=None):
    # Get the metrics_index's list of metrics indices
    # by loading the index as a module.
    from os import path
    import sys

    sys.path.append(path.join(path.dirname(__file__), path.pardir))
    from metrics_index import metrics_yamls

    from glean_parser import data_review
    from pathlib import Path

    return data_review.generate(
        bug, [Path(command_context.topsrcdir) / x for x in metrics_yamls]
    )
