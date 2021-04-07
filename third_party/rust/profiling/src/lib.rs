#[cfg(feature = "profile-with-puffin")]
pub use puffin;

#[cfg(feature = "profile-with-optick")]
pub use optick;

#[cfg(feature = "profile-with-superluminal")]
pub use superluminal_perf;

#[cfg(feature = "profile-with-tracing")]
pub use tracing;

#[cfg(feature = "profile-with-tracy")]
pub use tracy_client;

/// Opens a scope. Two variants:
///  - profiling::scope!(name: &str) - Opens a scope with the given name
///  - profiling::scope!(name: &str, data: &str) - Opens a scope with the given name and an extra
///    datafield. Details of this depend on the API, but it should be a &str. If the extra data is
///    named, it will be named "tag". Some APIs support adding more data (for example, `optic::tag!`)
///
/// ```
/// profiling::scope!("outer");
/// for _ in 0..10 {
///     profiling::scope!("inner", format!("iteration {}").as_str());
/// }
/// ```
#[macro_export]
macro_rules! scope {
    ($name:expr) => {
        #[cfg(feature = "profile-with-puffin")]
        $crate::puffin::profile_scope!($name);

        #[cfg(feature = "profile-with-optick")]
        $crate::optick::event!($name);

        #[cfg(feature = "profile-with-superluminal")]
        let _superluminal_guard = $crate::superluminal::SuperluminalGuard::new($name);

        #[cfg(feature = "profile-with-tracy")]
        // Note: callstack_depth is 0 since this has significant overhead
        let _tracy_span = $crate::tracy_client::Span::new($name, "", file!(), line!(), 0);

        #[cfg(feature = "profile-with-tracing")]
        let _span = $crate::tracing::span!(tracing::Level::INFO, $name);
        #[cfg(feature = "profile-with-tracing")]
        let _span_entered = _span.enter();
    };
    // NOTE: I've not been able to get attached data to work with optick
    ($name:expr, $data:expr) => {
        #[cfg(feature = "profile-with-puffin")]
        $crate::puffin::profile_scope!($name, $data);

        #[cfg(feature = "profile-with-optick")]
        $crate::optick::event!($name);
        #[cfg(feature = "profile-with-optick")]
        $crate::optick::tag!("tag", $data);

        #[cfg(feature = "profile-with-superluminal")]
        let _superluminal_guard =
            $crate::superluminal::SuperluminalGuard::new_with_data($name, $data);

        #[cfg(feature = "profile-with-tracy")]
        let _tracy_span = $crate::tracy_client::Span::new($name, "", file!(), line!(), 0);
        #[cfg(feature = "profile-with-tracy")]
        _tracy_span.emit_text($data);

        #[cfg(feature = "profile-with-tracing")]
        let _span = $crate::tracing::span!(tracing::Level::INFO, $name, tag = $data);
        #[cfg(feature = "profile-with-tracing")]
        let _span_entered = _span.enter();
    };
}

/// Proc macro for creating a scope around the function, using the name of the function for the
/// scope's name
///
/// This must be done as a proc macro because tracing requires a const string
///
/// ```
/// #[profiling::function]
/// fn my_function() {
///
/// }
/// ```
#[cfg(feature = "procmacros")]
pub use profiling_procmacros::function;

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
        // puffin uses the thread name

        #[cfg(feature = "profile-with-optick")]
        $crate::optick::register_thread($name);

        #[cfg(feature = "profile-with-superluminal")]
        $crate::superluminal_perf::set_current_thread_name($name);

        #[cfg(feature = "profile-with-tracy")]
        $crate::tracy_client::set_thread_name($name);
    };
}

/// Finishes the frame. This isn't strictly necessary for some kinds of applications but a pretty
/// normal thing to track in games.
#[macro_export]
macro_rules! finish_frame {
    () => {
        #[cfg(feature = "profile-with-puffin")]
        $crate::puffin::GlobalProfiler::lock().new_frame();

        #[cfg(feature = "profile-with-optick")]
        $crate::optick::next_frame();

        // superluminal does not have a frame end function

        #[cfg(feature = "profile-with-tracy")]
        $crate::tracy_client::finish_continuous_frame!();
    };
}

//
// RAII wrappers to support superluminal. These are public as they need to be callable from macros
// but are not intended for direct use.
//
#[cfg(feature = "profile-with-superluminal")]
#[doc(hidden)]
pub mod superluminal {
    pub struct SuperluminalGuard;

    // 0xFFFFFFFF means "use default color"
    const DEFAULT_SUPERLUMINAL_COLOR: u32 = 0xFFFFFFFF;

    impl SuperluminalGuard {
        pub fn new(name: &str) -> Self {
            superluminal_perf::begin_event(name);
            SuperluminalGuard
        }

        pub fn new_with_data(
            name: &str,
            data: &str,
        ) -> Self {
            superluminal_perf::begin_event_with_data(name, data, DEFAULT_SUPERLUMINAL_COLOR);
            SuperluminalGuard
        }
    }

    impl Drop for SuperluminalGuard {
        fn drop(&mut self) {
            superluminal_perf::end_event();
        }
    }
}
