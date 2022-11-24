import mozunit
import pytest
from mozfile import which

LINTER = "rst"
pytestmark = pytest.mark.skipif(
    not which("rstcheck"), reason="rstcheck is not installed"
)


def test_basic(lint, paths):
    results = lint(paths())
    assert len(results) == 2

    assert "Title underline too short" in results[0].message
    assert results[0].level == "error"
    assert results[0].relpath == "bad.rst"

    assert "Title overline & underline mismatch" in results[1].message
    assert results[1].level == "error"
    assert results[1].relpath == "bad2.rst"


if __name__ == "__main__":
    mozunit.main()
