use {Context, Result};
use std::ffi::CString;

pub fn init<T: Into<Vec<u8>>>(name: T) -> Result<Context> {
    let name = CString::new(name)?;

    Context::init(Some(name.as_c_str()), None)
}
