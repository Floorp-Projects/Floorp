#[test]
fn test_deserialise_byte_slice() {
    let val: &[u8] = &[0_u8, 1_u8, 2_u8, 3_u8];
    let ron = ron::to_string(val).unwrap();
    assert_eq!(ron, "[0,1,2,3]");

    // deserialising a byte slice from a byte sequence should fail
    //  with the error that a borrowed slice was expected but a byte
    //  buffer was provided
    // NOT with an expected string error, since the byte slice
    //  serialisation never serialises to a string
    assert_eq!(
        ron::from_str::<&[u8]>(&ron),
        Err(ron::error::SpannedError {
            code: ron::error::Error::InvalidValueForType {
                expected: String::from("a borrowed byte array"),
                found: String::from("the bytes \"AAECAw==\""),
            },
            position: ron::error::Position { line: 1, col: 10 },
        })
    );
}
