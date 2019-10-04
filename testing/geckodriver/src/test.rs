pub fn assert_de<T>(data: &T, json: serde_json::Value)
where
    T: std::fmt::Debug,
    T: std::cmp::PartialEq,
    T: serde::de::DeserializeOwned,
{
    assert_eq!(data, &serde_json::from_value::<T>(json).unwrap());
}
