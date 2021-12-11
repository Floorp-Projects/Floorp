import mozunit

from conftest import build

LINTER = "clang-format"
fixed = 0


def test_good(lint, config, paths):
    results = lint(paths("good/"), root=build.topsrcdir, use_filters=False)
    print(results)
    assert len(results) == 0

    results = lint(paths("good/"), root=build.topsrcdir, use_filters=False, fix=True)
    assert fixed == len(results)


def test_basic(lint, config, paths):
    results = lint(paths("bad/bad.cpp"), root=build.topsrcdir, use_filters=False)
    print(results)
    assert len(results) >= 1

    assert "Reformat C/C++" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 1
    assert results[0].column == 4
    assert "bad.cpp" in results[0].path
    assert "int  main ( ) {" in results[0].diff


def test_dir(lint, config, paths):
    results = lint(paths("bad/"), root=build.topsrcdir, use_filters=False)
    print(results)
    assert len(results) >= 4

    assert "Reformat C/C++" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 1
    assert results[0].column == 4
    assert "bad.cpp" in results[0].path
    assert "int  main ( ) {" in results[0].diff

    assert "Reformat C/C++" in results[5].message
    assert results[5].level == "warning"
    assert results[5].lineno == 1
    assert results[5].column == 18
    assert "bad2.c" in results[5].path
    assert "#include" in results[5].diff


def test_fixed(lint, create_temp_file):

    contents = """int  main ( ) { \n
return 0; \n

}"""

    path = create_temp_file(contents, "ignore.cpp")
    lint([path], use_filters=False, fix=True)

    assert fixed == 5


if __name__ == "__main__":
    mozunit.main()
