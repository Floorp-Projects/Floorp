/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub fn assert_de<T>(data: &T, json: serde_json::Value)
where
    T: std::fmt::Debug,
    T: std::cmp::PartialEq,
    T: serde::de::DeserializeOwned,
{
    assert_eq!(data, &serde_json::from_value::<T>(json).unwrap());
}
