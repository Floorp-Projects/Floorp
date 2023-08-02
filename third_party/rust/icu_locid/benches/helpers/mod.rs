// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod macros;

use std::fs::File;
use std::io::{BufReader, Error};

pub fn read_fixture<T>(path: &str) -> Result<T, Error>
where
    T: serde::de::DeserializeOwned,
{
    let file = File::open(path)?;
    let reader = BufReader::new(file);
    Ok(serde_json::from_reader(reader)?)
}
