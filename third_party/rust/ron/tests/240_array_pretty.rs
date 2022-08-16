use ron::ser::{to_string_pretty, PrettyConfig};

#[test]
fn small_array() {
    let arr = &[(), (), ()][..];
    assert_eq!(
        to_string_pretty(&arr, PrettyConfig::new().new_line("\n".to_string())).unwrap(),
        "[
    (),
    (),
    (),
]"
    );
    assert_eq!(
        to_string_pretty(
            &arr,
            PrettyConfig::new()
                .new_line("\n".to_string())
                .compact_arrays(true)
        )
        .unwrap(),
        "[(), (), ()]"
    );
    assert_eq!(
        to_string_pretty(
            &arr,
            PrettyConfig::new()
                .new_line("\n".to_string())
                .compact_arrays(true)
                .separator("".to_string())
        )
        .unwrap(),
        "[(),(),()]"
    );
    assert_eq!(
        to_string_pretty(
            &vec![(1, 2), (3, 4)],
            PrettyConfig::new()
                .new_line("\n".to_string())
                .separate_tuple_members(true)
                .compact_arrays(true)
        )
        .unwrap(),
        "[(
    1,
    2,
), (
    3,
    4,
)]"
    );
}
