# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

LINTER = "test-manifest-alpha"


def test_very_out_of_order(lint, paths):
    results = lint(paths("mochitest-very-out-of-order.ini"))
    assert len(results) == 1
    assert results[0].diff


def test_in_order(lint, paths):
    results = lint(paths("mochitest-in-order.ini"))
    assert len(results) == 0


def test_mostly_in_order(lint, paths):
    results = lint(paths("mochitest-mostly-in-order.ini"))
    assert len(results) == 1
    assert results[0].diff


def test_other_ini_very_out_of_order(lint, paths):
    """Test that an .ini file outside of the allowlist is ignored."""
    results = lint(paths("other-ini-very-out-of-order.ini"))
    assert len(results) == 0


if __name__ == "__main__":
    mozunit.main()
