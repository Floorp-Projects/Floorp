#[cfg(all(target_arch = "wasm32", feature = "wasmbind"))]
mod test {
    extern crate chrono;
    extern crate wasm_bindgen_test;

    use self::chrono::prelude::*;
    use self::wasm_bindgen_test::*;

    #[wasm_bindgen_test]
    fn now() {
        let utc: DateTime<Utc> = Utc::now();
        let local: DateTime<Local> = Local::now();

        // Ensure time fetched is correct
        let actual = Utc.datetime_from_str(env!("NOW"), "%s").unwrap();
        assert!(utc - actual < chrono::Duration::minutes(5));

        // Ensure offset retrieved when getting local time is correct
        let expected_offset = match env!("TZ") {
            "ACST-9:30" => FixedOffset::east(19 * 30 * 60),
            "Asia/Katmandu" => FixedOffset::east(23 * 15 * 60), // No DST thankfully
            "EST4" => FixedOffset::east(-4 * 60 * 60),
            "UTC0" => FixedOffset::east(0),
            _ => panic!("unexpected TZ"),
        };
        assert_eq!(&expected_offset, local.offset());
    }
}
