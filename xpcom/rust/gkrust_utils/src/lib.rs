extern crate nsstring;
extern crate uuid;
use nsstring::nsACString;
use uuid::Uuid;

use std::fmt::Write;

#[no_mangle]
pub extern "C" fn GkRustUtils_GenerateUUID(res: &mut nsACString) {
    let uuid = Uuid::new_v4().to_hyphenated();
    write!(res, "{{{}}}", uuid).expect("Unexpected uuid generated");
}
