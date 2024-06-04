/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::path::Path;

use nserror::{nsresult, NS_OK};
use nsstring::{nsAString, nsCString};

mod reader;
use reader::BuildIdReader;

use log::{error, trace};

#[no_mangle]
pub extern "C" fn read_toolkit_buildid_from_file(
    fname: &nsAString,
    nname: &nsCString,
    rv_build_id: &mut nsCString,
) -> nsresult {
    let fname_str = fname.to_string();
    let path = Path::new(&fname_str);
    let note_name = nname.to_string();

    trace!("read_toolkit_buildid_from_file {} {}", fname, nname);

    match BuildIdReader::new(&path) {
        Ok(mut reader) => match reader.read_string_build_id(&note_name) {
            Ok(id) => {
                trace!("read_toolkit_buildid_from_file {}", id);
                rv_build_id.assign(&id);
                NS_OK
            }
            Err(err) => {
                error!("read_toolkit_buildid_from_file failed to read string buiild id from note {:?} with error {:?}", note_name, err);
                err
            }
        },
        Err(err) => {
            error!("read_toolkit_buildid_from_file failed to build BuildIdReader for {:?} with error {:?}", path, err);
            err
        }
    }
}
