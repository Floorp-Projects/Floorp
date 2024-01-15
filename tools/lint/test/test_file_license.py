import mozunit

LINTER = "license"
fixed = 0


def test_lint_license(lint, paths):
    results = lint(paths())
    print(results)
    assert len(results) == 3

    assert ".eslintrc.js" in results[0].relpath

    assert "No matching license strings" in results[1].message
    assert results[1].level == "error"
    assert "bad.c" in results[1].relpath

    assert "No matching license strings" in results[2].message
    assert results[2].level == "error"
    assert "bad.js" in results[2].relpath


def test_lint_license_fix(lint, paths, create_temp_file):
    contents = """let foo = 0;"""
    path = create_temp_file(contents, "lint_license_test_tmp_file.js")
    results = lint([path], fix=True)

    assert len(results) == 0
    assert fixed == 1


if __name__ == "__main__":
    mozunit.main()
