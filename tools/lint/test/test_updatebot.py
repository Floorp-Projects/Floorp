import os

import mozunit

LINTER = "updatebot"


def test_basic(lint, paths):
    results = []

    for p in paths():
        for root, dirs, files in os.walk(p):
            for f in files:
                if f == ".yamllint":
                    continue

                filepath = os.path.join(root, f)
                result = lint(filepath, testing=True)
                if result:
                    results.append(result)

    assert len(results) == 2

    expected_results = 0

    for r in results:
        if "no-revision.yaml" in r[0].path:
            expected_results += 1
            assert "no-revision.yaml" in r[0].path
            assert (
                'If "vendoring" is present, "revision" must be present in "origin"'
                in r[0].message
            )

        if "cargo-mismatch.yaml" in r[0].path:
            expected_results += 1
            assert "cargo-mismatch.yaml" in r[0].path
            assert "wasn't found in Cargo.lock" in r[0].message

    assert expected_results == 2


if __name__ == "__main__":
    mozunit.main()
