# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

LINTER = "fluent-lint"


def test_lint_exclusions(lint, paths):
    results = lint(paths("excluded.ftl"))
    assert len(results) == 1
    assert results[0].rule == "TE01"
    assert results[0].lineno == 6
    assert results[0].column == 20


def test_lint_single_file(lint, paths):
    results = lint(paths("bad.ftl"))
    assert len(results) == 11
    assert results[0].rule == "ID01"
    assert results[0].lineno == 1
    assert results[0].column == 1
    assert results[1].rule == "ID01"
    assert results[1].lineno == 3
    assert results[1].column == 1
    assert results[2].rule == "TE01"
    assert results[2].lineno == 5
    assert results[2].column == 20
    assert results[3].rule == "TE01"
    assert results[3].lineno == 6
    assert results[3].column == 24
    assert results[4].rule == "TE02"
    assert results[4].lineno == 7
    assert results[4].column == 20
    assert results[5].rule == "TE03"
    assert results[5].lineno == 8
    assert results[5].column == 20
    assert results[6].rule == "TE04"
    assert results[6].lineno == 11
    assert results[6].column == 20
    assert results[7].rule == "TE05"
    assert results[7].lineno == 13
    assert results[7].column == 16
    assert results[8].rule == "TE03"
    assert results[8].lineno == 17
    assert results[8].column == 20
    assert results[9].rule == "TE03"
    assert results[9].lineno == 25
    assert results[9].column == 18
    assert results[10].rule == "ID02"
    assert results[10].lineno == 32
    assert results[10].column == 1


def test_comment_group(lint, paths):
    results = lint(paths("comment-group1.ftl"))
    assert len(results) == 6
    assert results[0].rule == "GC03"
    assert results[0].lineno == 12
    assert results[0].column == 1
    assert results[1].rule == "GC02"
    assert results[1].lineno == 16
    assert results[1].column == 1
    assert results[2].rule == "GC04"
    assert results[2].lineno == 21
    assert results[2].column == 1
    assert results[3].rule == "GC03"
    assert results[3].lineno == 26
    assert results[3].column == 1
    assert results[4].rule == "GC02"
    assert results[4].lineno == 30
    assert results[4].column == 1
    assert results[5].rule == "GC01"
    assert results[5].lineno == 35
    assert results[5].column == 1

    results = lint(paths("comment-group2.ftl"))
    assert (len(results)) == 0


def test_comment_resource(lint, paths):
    results = lint(paths("comment-resource1.ftl"))
    assert len(results) == 1
    assert results[0].rule == "RC01"
    assert results[0].lineno == 9
    assert results[0].column == 1

    results = lint(paths("comment-resource2.ftl"))
    assert len(results) == 1
    assert results[0].rule == "RC03"
    assert results[0].lineno == 4
    assert results[0].column == 1

    results = lint(paths("comment-resource3.ftl"))
    assert len(results) == 1
    assert results[0].rule == "RC02"
    assert results[0].lineno == 5
    assert results[0].column == 1

    results = lint(paths("comment-resource4.ftl"))
    assert len(results) == 1
    assert results[0].rule == "RC03"
    assert results[0].lineno == 6
    assert results[0].column == 1

    results = lint(paths("comment-resource5.ftl"))
    assert len(results) == 1
    assert results[0].rule == "RC02"
    assert results[0].lineno == 5
    assert results[0].column == 1

    results = lint(paths("comment-resource6.ftl"))
    assert len(results) == 0


def test_brand_names(lint, paths):
    results = lint(paths("brand-names.ftl"))
    assert len(results) == 11
    assert results[0].rule == "CO01"
    assert results[0].lineno == 1
    assert results[0].column == 16
    assert "Firefox" in results[0].message
    assert "Mozilla" not in results[0].message
    assert "Thunderbird" not in results[0].message
    assert results[1].rule == "CO01"
    assert results[1].lineno == 4
    assert results[1].column == 16

    results = lint(paths("brand-names-excluded.ftl"))
    assert len(results) == 0


if __name__ == "__main__":
    mozunit.main()
