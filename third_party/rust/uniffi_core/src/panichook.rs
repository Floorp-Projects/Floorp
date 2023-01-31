/// Initialize our panic handling hook to optionally log panics
#[cfg(feature = "log_panics")]
pub fn ensure_setup() {
    use std::sync::Once;
    static INIT_BACKTRACES: Once = Once::new();
    INIT_BACKTRACES.call_once(move || {
        #[cfg(all(feature = "log_backtraces", not(target_os = "android")))]
        {
            std::env::set_var("RUST_BACKTRACE", "1");
        }
        // Turn on a panic hook which logs both backtraces and the panic
        // "Location" (file/line). We do both in case we've been stripped,
        // ).
        std::panic::set_hook(Box::new(move |panic_info| {
            let (file, line) = if let Some(loc) = panic_info.location() {
                (loc.file(), loc.line())
            } else {
                // Apparently this won't happen but rust has reserved the
                // ability to start returning None from location in some cases
                // in the future.
                ("<unknown>", 0)
            };
            log::error!("### Rust `panic!` hit at file '{file}', line {line}");
            #[cfg(all(feature = "log_backtraces", not(target_os = "android")))]
            {
                log::error!("  Complete stack trace:\n{:?}", backtrace::Backtrace::new());
            }
        }));
    });
}

/// Initialize our panic handling hook to optionally log panics
#[cfg(not(feature = "log_panics"))]
pub fn ensure_setup() {}
