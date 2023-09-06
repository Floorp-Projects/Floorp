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
    assert len(results) == 1

    assert "Reformat C/C++" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 1
    assert results[0].column == 0
    assert "bad.cpp" in results[0].path
    assert (
        results[0].diff
        == """\
-int  main ( ) { 
-
-return 0;   
-
-
-}
+int main() { return 0; }
"""  # noqa
    )


def test_dir(lint, config, paths):
    results = lint(paths("bad/"), root=build.topsrcdir, use_filters=False)
    print(results)
    assert len(results) == 5

    assert "Reformat C/C++" in results[0].message
    assert results[0].level == "warning"
    assert results[0].lineno == 1
    assert results[0].column == 0
    assert "bad.cpp" in results[0].path
    assert (
        results[0].diff
        == """\
-int  main ( ) { 
-
-return 0;   
-
-
-}
+int main() { return 0; }
"""  # noqa
    )

    assert "Reformat C/C++" in results[1].message
    assert results[1].level == "warning"
    assert results[1].lineno == 1
    assert results[1].column == 0
    assert "bad2.c" in results[1].path
    assert (
        results[1].diff
        == """\
-#include "bad2.h"
-
-
-int bad2() {
+#include "bad2.h"
+
+int bad2() {
"""
    )

    assert "Reformat C/C++" in results[2].message
    assert results[2].level == "warning"
    assert results[2].lineno == 5
    assert results[2].column == 0
    assert "bad2.c" in results[2].path
    assert (
        results[2].diff
        == """\
-  int a =2;
+  int a = 2;
"""
    )

    assert "Reformat C/C++" in results[3].message
    assert results[3].level == "warning"
    assert results[3].lineno == 6
    assert results[3].column == 0
    assert "bad2.c" in results[3].path
    assert (
        results[3].diff
        == """\
-  return a;
-
-}
+  return a;
+}
"""
    )

    assert "Reformat C/C++" in results[4].message
    assert results[4].level == "warning"
    assert results[4].lineno == 1
    assert results[4].column == 0
    assert "bad2.h" in results[4].path
    assert (
        results[4].diff
        == """\
-int bad2(void );
+int bad2(void);
"""
    )


def test_fixed(lint, create_temp_file):
    contents = """int  main ( ) { \n
return 0; \n

}"""

    path = create_temp_file(contents, "ignore.cpp")
    lint([path], use_filters=False, fix=True)

    assert fixed == 1


if __name__ == "__main__":
    mozunit.main()
