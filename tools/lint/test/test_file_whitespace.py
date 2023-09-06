import mozunit

LINTER = "file-whitespace"
fixed = 0


def test_lint_file_whitespace(lint, paths):
    results = lint(paths())
    print(results)
    assert len(results) == 5

    assert "File does not end with newline character" in results[1].message
    assert results[1].level == "error"
    assert "bad-newline.c" in results[1].relpath

    assert "Empty Lines at end of file" in results[0].message
    assert results[0].level == "error"
    assert "bad-newline.c" in results[0].relpath

    assert "Windows line return" in results[2].message
    assert results[2].level == "error"
    assert "bad-windows.c" in results[2].relpath

    assert "Trailing whitespace" in results[3].message
    assert results[3].level == "error"
    assert "bad.c" in results[3].relpath
    assert results[3].lineno == 1

    assert "Trailing whitespace" in results[4].message
    assert results[4].level == "error"
    assert "bad.c" in results[4].relpath
    assert results[4].lineno == 2


def test_lint_file_whitespace_fix(lint, paths, create_temp_file):
    contents = """int main() {  \n
    return 0;  \n
}


"""

    path = create_temp_file(contents, "bad.cpp")
    lint([path], fix=True)
    # Gives a different answer on Windows. Probably because of Windows CR
    assert fixed == 3 or fixed == 2


if __name__ == "__main__":
    mozunit.main()
