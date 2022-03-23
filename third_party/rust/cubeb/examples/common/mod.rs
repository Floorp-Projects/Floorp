use cubeb::{Context, Result};
use std::env;
use std::ffi::CString;
use std::io::{self, Write};

pub fn init<T: Into<Vec<u8>>>(ctx_name: T) -> Result<Context> {
    let backend = match env::var("CUBEB_BACKEND") {
        Ok(s) => Some(s),
        Err(_) => None,
    };

    let ctx_name = CString::new(ctx_name).unwrap();
    let ctx = Context::init(Some(ctx_name.as_c_str()), None);
    if let Ok(ref ctx) = ctx {
        if let Some(ref backend) = backend {
            let ctx_backend = ctx.backend_id();
            if backend != ctx_backend {
                let stderr = io::stderr();
                let mut handle = stderr.lock();

                writeln!(
                    handle,
                    "Requested backend `{}', got `{}'",
                    backend, ctx_backend
                )
                .unwrap();
            }
        }
    }

    ctx
}
