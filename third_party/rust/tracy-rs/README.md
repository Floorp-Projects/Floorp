# tracy-rs

This provides a Rust integration for the Tracy profiling library (https://bitbucket.org/wolfpld/tracy).

## Usage

1) Clone the Tracy library from the above URL.
2) Follow the steps to build the profiler GUI (e.g. in tracy/profiler/build/unix/).
3) Follow the steps to build the profiler shared library (e.g. in tracy/library/unix).
4) Add this crate to your project dependencies, with the 'enable_profiler' cargo feature.
5) Call `tracy_rs::load` at the start of your application, providing path to library from (3).
6) Insert the main frame marker `tracy_frame_marker!();` at the end of your frame.
7) Optionally, add sub-frame markers with `tracy_begin_frame!()` and `tracy_end_frame!()`.
8) Annotate functions to be profiled with `profile_scope!()`.
9) Run the application and profiler GUI.
