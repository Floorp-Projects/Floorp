#[cfg(feature = "std")]
mod std_tests {
    use serde_cbor::value::Value;

    #[test]
    fn integer_canonical_sort_order() {
        let expected = [
            0,
            23,
            24,
            255,
            256,
            65535,
            65536,
            4294967295,
            -1,
            -24,
            -25,
            -256,
            -257,
            -65536,
            -65537,
            -4294967296,
        ]
        .iter()
        .map(|i| Value::Integer(*i))
        .collect::<Vec<_>>();

        let mut sorted = expected.clone();
        sorted.sort();

        assert_eq!(expected, sorted);
    }

    #[test]
    fn string_canonical_sort_order() {
        let expected = ["", "a", "b", "aa"]
            .iter()
            .map(|s| Value::Text(s.to_string()))
            .collect::<Vec<_>>();

        let mut sorted = expected.clone();
        sorted.sort();

        assert_eq!(expected, sorted);
    }

    #[test]
    fn bytes_canonical_sort_order() {
        let expected = vec![vec![], vec![0u8], vec![1u8], vec![0u8, 0u8]]
            .into_iter()
            .map(|v| Value::Bytes(v))
            .collect::<Vec<_>>();

        let mut sorted = expected.clone();
        sorted.sort();

        assert_eq!(expected, sorted);
    }

    #[test]
    fn simple_data_canonical_sort_order() {
        let expected = vec![Value::Bool(false), Value::Bool(true), Value::Null];

        let mut sorted = expected.clone();
        sorted.sort();

        assert_eq!(expected, sorted);
    }

    #[test]
    fn major_type_canonical_sort_order() {
        let expected = vec![
            Value::Integer(0),
            Value::Integer(-1),
            Value::Bytes(vec![]),
            Value::Text("".to_string()),
            Value::Null,
        ];

        let mut sorted = expected.clone();
        sorted.sort();

        assert_eq!(expected, sorted);
    }

    #[test]
    fn test_rfc_example() {
        // See: https://tools.ietf.org/html/draft-ietf-cbor-7049bis-04#section-4.10
        let expected = vec![
            Value::Integer(10),
            Value::Integer(100),
            Value::Integer(-1),
            Value::Text("z".to_owned()),
            Value::Text("aa".to_owned()),
            Value::Array(vec![Value::Integer(100)]),
            Value::Array(vec![Value::Integer(-1)]),
            Value::Bool(false),
        ];
        let mut sorted = expected.clone();
        sorted.sort();
        assert_eq!(expected, sorted);
    }
}
