const EXPTECTED: &str = "[
    /*[0]*/ None,
    /*[1]*/ Some([]),
    /*[2]*/ Some([
        /*[0]*/ 42,
    ]),
    /*[3]*/ Some([
        /*[0]*/ 4,
        /*[1]*/ 2,
    ]),
    /*[4]*/ None,
]";

const EXPTECTED_COMPACT: &str = "[/*[0]*/ None, /*[1]*/ Some([]), /*[2]*/ Some([/*[0]*/ 42]), \
/*[3]*/ Some([/*[0]*/ 4, /*[1]*/ 2]), /*[4]*/ None]";

#[test]
fn enumerate_arrays() {
    let v: Vec<Option<Vec<u8>>> = vec![None, Some(vec![]), Some(vec![42]), Some(vec![4, 2]), None];

    let pretty = ron::ser::PrettyConfig::new().enumerate_arrays(true);

    let ser = ron::ser::to_string_pretty(&v, pretty).unwrap();

    assert_eq!(ser, EXPTECTED);

    let de: Vec<Option<Vec<u8>>> = ron::from_str(&ser).unwrap();

    assert_eq!(v, de)
}

#[test]
fn enumerate_compact_arrays() {
    let v: Vec<Option<Vec<u8>>> = vec![None, Some(vec![]), Some(vec![42]), Some(vec![4, 2]), None];

    let pretty = ron::ser::PrettyConfig::new()
        .enumerate_arrays(true)
        .compact_arrays(true);

    let ser = ron::ser::to_string_pretty(&v, pretty).unwrap();

    assert_eq!(ser, EXPTECTED_COMPACT);

    let de: Vec<Option<Vec<u8>>> = ron::from_str(&ser).unwrap();

    assert_eq!(v, de)
}
