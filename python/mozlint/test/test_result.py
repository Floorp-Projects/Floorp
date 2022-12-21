# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

from mozlint.result import Issue, ResultSummary


def test_issue_defaults():
    ResultSummary.root = "/fake/root"

    issue = Issue(linter="linter", path="path", message="message", lineno=None)
    assert issue.lineno == 0
    assert issue.column is None
    assert issue.level == "error"

    issue = Issue(
        linter="linter", path="path", message="message", lineno="1", column="2"
    )
    assert issue.lineno == 1
    assert issue.column == 2


if __name__ == "__main__":
    mozunit.main()
