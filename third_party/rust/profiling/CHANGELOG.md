# Changelog

## 0.1.9
 * Bump tracy client version to 0.12 (tracy now uses the 0.7.6 protocol)
 * Bump puffin to 0.4
 * Update the puffin demo to use rafx
 * Aesthetic changes to the puffin example

## 0.1.8
 * Bump tracy client version to 0.11 (tracy now uses the 0.7.5 protocol)

## 0.1.7
 * Re-release due to root cargo.toml referencing 0.1.2 of procmacro crate instead of 0.1.3

## 0.1.6
 * Use tracy directly instead of going through tracy (use the profile-with-tracy feature).
 * Going through tracing is still possible and demonstrated in the example (use the profile-with-tracing feature).
 * Function instrumentation is now recorded to superluminal

## 0.1.5
 * More info in readme, no functional change

## 0.1.4
 * Fix the color passed to superluminal so that scopes are default-colored

## 0.1.3
 * Re-export the profiler crates, simplifying changes needed in end-user's cargo.toml

## 0.1.2
 * Remove unintended and unnecessary dependencies from the procmacro crate
 * Republish the bindings crate mainly to fix readme typos and add a bit more info about the exposed APIs

## 0.1.1
 * Add profiling::function procmacro
 * Add profiling::register_thread!()
 * Add profiling::finish_frame!()
 * Add support for superluminal
 * Fixed incorrect usage of span!() that affected tracy captures
 * More examples, improved documentation

## 0.1.0
 * Initial release