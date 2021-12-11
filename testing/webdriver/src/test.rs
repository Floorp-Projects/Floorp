/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub fn assert_ser_de<T>(data: &T, json: serde_json::Value)
where
    T: std::fmt::Debug,
    T: std::cmp::PartialEq,
    T: serde::de::DeserializeOwned,
    T: serde::Serialize,
{
    assert_eq!(serde_json::to_value(data).unwrap(), json);
    assert_eq!(data, &serde_json::from_value::<T>(json).unwrap());
}

pub fn assert_ser<T>(data: &T, json: serde_json::Value)
where
    T: std::fmt::Debug,
    T: std::cmp::PartialEq,
    T: serde::Serialize,
{
    assert_eq!(serde_json::to_value(data).unwrap(), json);
}

pub fn assert_de<T>(data: &T, json: serde_json::Value)
where
    T: std::fmt::Debug,
    T: std::cmp::PartialEq,
    T: serde::de::DeserializeOwned,
{
    assert_eq!(data, &serde_json::from_value::<T>(json).unwrap());
}
