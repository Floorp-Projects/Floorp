#[macro_export]
macro_rules! scope {
    ($name:expr) => {
        // Note: callstack_depth is 0 since this has significant overhead
        let _tracy_span = $crate::tracy_client::Span::new($name, "", file!(), line!(), 0);
    };
    ($name:expr, $data:expr) => {
        let _tracy_span = $crate::tracy_client::Span::new($name, "", file!(), line!(), 0);
        _tracy_span.emit_text($data);
    };
}

/// Registers a thread with the profiler API(s). This is usually setting a name for the thread.
/// Two variants:
///  - register_thread!() - Tries to get the name of the thread, or an ID if no name is set
///  - register_thread!(name: &str) - Registers the thread using the given name
#[macro_export]
macro_rules! register_thread {
    () => {
        let thread_name = std::thread::current()
            .name()
            .map(|x| x.to_string())
            .unwrap_or_else(|| format!("Thread {:?}", std::thread::current().id()));

        $crate::register_thread!(&thread_name);
    };
    ($name:expr) => {
        $crate::tracy_client::set_thread_name($name);
    };
}

/// Finishes the frame. This isn't strictly necessary for some kinds of applications but a pretty
/// normal thing to track in games.
#[macro_export]
macro_rules! finish_frame {
    () => {
        $crate::tracy_client::finish_continuous_frame!();
    };
}
