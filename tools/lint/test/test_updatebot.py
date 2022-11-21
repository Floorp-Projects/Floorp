import os
import mozunit

LINTER = "updatebot"


def test_basic(lint, paths):
    results = []

    for p in paths():
        for (root, dirs, files) in os.walk(p):
            for f in files:
                if f == ".yamllint":
                    continue

                filepath = os.path.join(root, f)
                result = lint(filepath, testing=True)
                if result:
                    results.append(result)

    assert len(results) == 2

    i = 0
    assert "no-revision.yaml" in results[i][0].path
    assert (
        'If "vendoring" is present, "revision" must be present in "origin"'
        in results[i][0].message
    )

    i += 1
    assert "cargo-mismatch.yaml" in results[i][0].path
    assert "wasn't found in Cargo.lock" in results[i][0].message


if __name__ == "__main__":
    mozunit.main()
