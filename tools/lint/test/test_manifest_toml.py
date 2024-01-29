# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

LINTER = "test-manifest-toml"
fixed = 0


def test_valid(lint, paths):
    results = lint(paths("valid.toml"))
    assert len(results) == 0


def test_invalid(lint, paths):
    results = lint(paths("invalid.toml"))
    assert len(results) == 1
    assert results[0].message == "The manifest is not valid TOML."


def test_no_default(lint, paths):
    """Test verifying [DEFAULT] section."""
    results = lint(paths("no-default.toml"))
    assert len(results) == 1
    assert results[0].message == "The manifest does not start with a [DEFAULT] section."


def test_no_default_fix(lint, paths, create_temp_file):
    """Test fixing missing [DEFAULT] section."""
    contents = "# this Manifest has no DEFAULT section\n"
    path = create_temp_file(contents, "no-default.toml")
    results = lint([path], fix=True)
    assert len(results) == 1
    assert results[0].message == "The manifest does not start with a [DEFAULT] section."
    assert fixed == 1


def test_non_double_quote_sections(lint, paths):
    """Test verifying [DEFAULT] section."""
    results = lint(paths("non-double-quote-sections.toml"))
    assert len(results) == 2
    assert results[0].message.startswith("The section name must be double quoted:")


def test_unsorted(lint, paths):
    """Test sections in alpha order."""
    results = lint(paths("unsorted.toml"))
    assert len(results) == 1
    assert results[0].message == "The manifest sections are not in alphabetical order."


def test_comment_section(lint, paths):
    """Test for commented sections."""
    results = lint(paths("comment-section.toml"))
    assert len(results) == 2
    assert results[0].message.startswith(
        "Use 'disabled = \"<reason>\"' to disable a test instead of a comment:"
    )


def test_skip_if_not_array(lint, paths):
    """Test for non-array skip-if value."""
    results = lint(paths("skip-if-not-array.toml"))
    assert len(results) == 1
    assert results[0].message.startswith("Value for conditional must be an array:")


def test_skip_if_explicit_or(lint, paths):
    """Test for explicit || in skip-if."""
    results = lint(paths("skip-if-explicit-or.toml"))
    assert len(results) == 1
    assert results[0].message.startswith(
        "Value for conditional must not include explicit ||, instead put on multiple lines:"
    )


if __name__ == "__main__":
    mozunit.main()
