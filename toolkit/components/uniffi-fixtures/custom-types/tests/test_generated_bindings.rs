uniffi::build_foreign_language_testcases!(
    ["src/custom-types.udl"],
    [
        "tests/bindings/test_custom_types.kts",
        "tests/bindings/test_custom_types.swift",
    ]
);
