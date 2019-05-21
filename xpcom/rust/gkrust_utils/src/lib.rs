extern crate nsstring;
extern crate uuid;
use nsstring::nsACString;
use uuid::Uuid;

use std::fmt::Write;

#[no_mangle]
pub extern "C" fn GkRustUtils_GenerateUUID(res: &mut nsACString) {
    // TODO once the vendored Uuid implementation is >7 this likely can use Hyphenated instead of to_string
    let uuid = Uuid::new_v4().hyphenated().to_string();
    write!(res, "{{{}}}", uuid).expect("Unexpected uuid generated");
}
