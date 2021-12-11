# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import defaultdict
from json import JSONEncoder
import os
import mozpack.path as mozpath

import attr


class ResultSummary(object):
    """Represents overall result state from an entire lint run."""

    root = None

    def __init__(self, root):
        self.reset()

        # Store the repository root folder to be able to build
        # Issues relative paths to that folder
        if ResultSummary.root is None:
            ResultSummary.root = mozpath.normpath(root)

    def reset(self):
        self.issues = defaultdict(list)
        self.failed_run = set()
        self.failed_setup = set()
        self.suppressed_warnings = defaultdict(int)
        self.fixed = 0

    @property
    def returncode(self):
        if self.issues or self.failed:
            return 1
        return 0

    @property
    def failed(self):
        return self.failed_setup | self.failed_run

    @property
    def total_issues(self):
        return sum([len(v) for v in self.issues.values()])

    @property
    def total_suppressed_warnings(self):
        return sum(self.suppressed_warnings.values())

    @property
    def total_fixed(self):
        return self.fixed

    def update(self, other):
        """Merge results from another ResultSummary into this one."""
        for path, obj in other.issues.items():
            self.issues[path].extend(obj)

        self.failed_run |= other.failed_run
        self.failed_setup |= other.failed_setup
        self.fixed += other.fixed
        for k, v in other.suppressed_warnings.items():
            self.suppressed_warnings[k] += v


@attr.s(slots=True, kw_only=True)
class Issue(object):
    """Represents a single lint issue and its related metadata.

    :param linter: name of the linter that flagged this error
    :param path: path to the file containing the error
    :param message: text describing the error
    :param lineno: line number that contains the error
    :param column: column containing the error
    :param level: severity of the error, either 'warning' or 'error' (default 'error')
    :param hint: suggestion for fixing the error (optional)
    :param source: source code context of the error (optional)
    :param rule: name of the rule that was violated (optional)
    :param lineoffset: denotes an error spans multiple lines, of the form
                       (<lineno offset>, <num lines>) (optional)
    :param diff: a diff describing the changes that need to be made to the code
    """

    linter = attr.ib()
    path = attr.ib()
    lineno = attr.ib(
        default=None, converter=lambda lineno: int(lineno) if lineno else 0
    )
    column = attr.ib(
        default=None, converter=lambda column: int(column) if column else column
    )
    message = attr.ib()
    hint = attr.ib(default=None)
    source = attr.ib(default=None)
    level = attr.ib(default=None, converter=lambda level: level or "error")
    rule = attr.ib(default=None)
    lineoffset = attr.ib(default=None)
    diff = attr.ib(default=None)
    relpath = attr.ib(init=False, default=None)

    def __attrs_post_init__(self):
        root = ResultSummary.root
        assert root is not None, "Missing ResultSummary.root"
        if os.path.isabs(self.path):
            self.path = mozpath.normpath(self.path)
            self.relpath = mozpath.relpath(self.path, root)
        else:
            self.relpath = mozpath.normpath(self.path)
            self.path = mozpath.join(root, self.path)


class IssueEncoder(JSONEncoder):
    """Class for encoding :class:`~result.Issue`s to json.

    Usage:

        json.dumps(results, cls=IssueEncoder)
    """

    def default(self, o):
        if isinstance(o, Issue):
            return attr.asdict(o)
        return JSONEncoder.default(self, o)


def from_config(config, **kwargs):
    """Create a :class:`~result.Issue` from a linter config.

    Convenience method that pulls defaults from a linter
    config and forwards them.

    :param config: linter config as defined in a .yml file
    :param kwargs: same as :class:`~result.Issue`
    :returns: :class:`~result.Issue` object
    """
    args = {}
    for arg in attr.fields(Issue):
        if arg.init:
            args[arg.name] = kwargs.get(arg.name, config.get(arg.name))

    if not args["linter"]:
        args["linter"] = config.get("name")

    if not args["message"]:
        args["message"] = config.get("description")

    return Issue(**args)
