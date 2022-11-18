import mozunit
from conftest import build

LINTER = "android-format"


def test_basic(global_lint, config):
    substs = {
        "GRADLE_ANDROID_FORMAT_LINT_CHECK_TASKS": [
            "spotlessJavaCheck",
            "spotlessKotlinCheck",
        ],
        "GRADLE_ANDROID_FORMAT_LINT_FIX_TASKS": [
            "spotlessJavaApply",
            "spotlessKotlinApply",
        ],
        "GRADLE_ANDROID_FORMAT_LINT_FOLDERS": ["tools/lint/test/files/android-format"],
    }
    results = global_lint(
        config=config,
        topobjdir=build.topobjdir,
        root=build.topsrcdir,
        substs=substs,
        extra_args=["-PandroidFormatLintTest"],
    )
    print(results)

    # When first task (spotlessJavaCheck) hits error, we won't check next Kotlin error.
    # So results length will be 1.
    assert len(results) == 1
    assert results[0].level == "error"

    # Since android-format is global lint, fix=True overrides repository files directly.
    # No way to add this test.


if __name__ == "__main__":
    mozunit.main()
