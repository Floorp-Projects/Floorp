use bitflags::bitflags;
use option_set::option_set;

bitflags! {
    #[derive(
        Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash,
        serde::Serialize, serde::Deserialize,
    )]
    struct TestGood: u8 {
        const ONE = 1;
        const TWO = 1 << 1;
        const THREE  = 1 << 2;
    }
}

option_set! {
    struct TestBad: UpperCamel + u8 {
        const ONE = 1;
        const TWO = 1 << 1;
        const THREE  = 1 << 2;
    }
}

#[test]
fn test_bitflags() {
    // Test case provided by jaynus in
    // https://github.com/ron-rs/ron/issues/152#issue-421298302

    let flag_good = TestGood::ONE | TestGood::TWO;

    let json_ser_good = serde_json::ser::to_string(&flag_good).unwrap();
    let ron_ser_good = ron::ser::to_string(&flag_good).unwrap();

    assert_eq!(json_ser_good, "\"ONE | TWO\"");
    assert_eq!(ron_ser_good, "(\"ONE | TWO\")");

    let json_de_good: TestGood = serde_json::de::from_str(json_ser_good.as_str()).unwrap();
    let ron_de_good: TestGood = ron::de::from_str(ron_ser_good.as_str()).unwrap();

    assert_eq!(json_de_good, flag_good);
    assert_eq!(ron_de_good, flag_good);

    // option_set
    let flag_bad = TestBad::ONE | TestBad::TWO;

    let json_ser_bad = serde_json::ser::to_string(&flag_bad).unwrap();
    let ron_ser_bad = ron::ser::to_string(&flag_bad).unwrap();

    assert_eq!(json_ser_bad, "[\"One\",\"Two\"]");
    assert_eq!(ron_ser_bad, "[\"One\",\"Two\"]");

    let json_de_bad: TestBad = serde_json::de::from_str(json_ser_bad.as_str()).unwrap();
    let ron_de_bad: TestBad = ron::de::from_str(ron_ser_bad.as_str()).unwrap();

    assert_eq!(json_de_bad, flag_bad);
    assert_eq!(ron_de_bad, flag_bad);
}
