import mozunit
import pytest
from mozfile import which

LINTER = "shellcheck"
pytestmark = pytest.mark.skipif(
    not which("shellcheck"), reason="shellcheck is not installed"
)


def test_basic(lint, paths):
    results = lint(paths())
    print(results)
    assert len(results) == 2

    assert "hello appears unused" in results[0].message
    assert results[0].level == "error"
    assert results[0].relpath == "bad.sh"

    assert "Double quote to prevent" in results[1].message
    assert results[1].level == "error"
    assert results[1].relpath == "bad.sh"


if __name__ == "__main__":
    mozunit.main()
