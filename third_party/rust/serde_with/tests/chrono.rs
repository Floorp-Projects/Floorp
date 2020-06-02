#![cfg(feature = "chrono")]

extern crate chrono;
extern crate pretty_assertions;
extern crate serde;
extern crate serde_derive;
extern crate serde_json;
extern crate serde_with;

use chrono::{DateTime, NaiveDateTime, Utc};
use pretty_assertions::assert_eq;
use serde_derive::Deserialize;

fn new_datetime(secs: i64, nsecs: u32) -> DateTime<Utc> {
    DateTime::from_utc(NaiveDateTime::from_timestamp(secs, nsecs), Utc)
}

#[test]
fn json_datetime_from_any_to_string_deserialization() {
    #[derive(Debug, Deserialize)]
    struct S {
        #[serde(with = "serde_with::chrono::datetime_utc_ts_seconds_from_any")]
        date: DateTime<Utc>,
    }
    let from = r#"[
        { "date": 1478563200 },
        { "date": 0 },
        { "date": -86000 },
        { "date": 1478563200.123 },
        { "date": 0.000 },
        { "date": -86000.999 },
        { "date": "1478563200.123" },
        { "date": "0.000" },
        { "date": "-86000.999" }
    ]"#;

    let res: Vec<S> = serde_json::from_str(from).unwrap();

    // just integers
    assert_eq!(new_datetime(1_478_563_200, 0), res[0].date);
    assert_eq!(new_datetime(0, 0), res[1].date);
    assert_eq!(new_datetime(-86000, 0), res[2].date);

    // floats, shows precision errors in subsecond part
    assert_eq!(new_datetime(1_478_563_200, 122_999_906), res[3].date);
    assert_eq!(new_datetime(0, 0), res[4].date);
    assert_eq!(new_datetime(-86000, 998_999_999), res[5].date);

    // string representation of floats
    assert_eq!(new_datetime(1_478_563_200, 123_000_000), res[6].date);
    assert_eq!(new_datetime(0, 0), res[7].date);
    assert_eq!(new_datetime(-86000, 999_000_000), res[8].date);
}
