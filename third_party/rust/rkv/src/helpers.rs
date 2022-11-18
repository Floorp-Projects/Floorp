// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{
    io,
    path::{Path, PathBuf},
};

use url::Url;

use crate::{error::StoreError, value::Value};

pub(crate) fn read_transform(value: Result<&[u8], StoreError>) -> Result<Value, StoreError> {
    match value {
        Ok(bytes) => Value::from_tagged_slice(bytes).map_err(StoreError::DataError),
        Err(e) => Err(e),
    }
}

// Workaround the UNC path on Windows, see https://github.com/rust-lang/rust/issues/42869.
// Otherwise, `Env::from_builder()` will panic with error_no(123).
pub(crate) fn canonicalize_path<'p, P>(path: P) -> io::Result<PathBuf>
where
    P: Into<&'p Path>,
{
    let canonical = path.into().canonicalize()?;

    Ok(if cfg!(target_os = "windows") {
        let map_err = |_| io::Error::new(io::ErrorKind::Other, "path canonicalization error");
        Url::from_file_path(&canonical)
            .and_then(|url| url.to_file_path())
            .map_err(map_err)?
    } else {
        canonical
    })
}
