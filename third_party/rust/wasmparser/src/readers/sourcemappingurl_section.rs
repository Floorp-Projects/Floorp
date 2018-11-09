/* Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use super::{BinaryReader, BinaryReaderError, Result};

pub(crate) fn read_sourcemappingurl_section_content<'a>(
    data: &'a [u8],
    offset: usize,
) -> Result<&'a [u8]> {
    let mut reader = BinaryReader::new_with_offset(data, offset);
    let url = reader.read_string()?;
    if !reader.eof() {
        return Err(BinaryReaderError {
            message: "Unexpected content in the sourceMappingURL section",
            offset: offset + reader.position,
        });
    }
    Ok(url)
}
