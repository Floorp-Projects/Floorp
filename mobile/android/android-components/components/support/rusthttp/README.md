# Rust HTTP

This essentially wraps the rust HTTP config library so that consumers who don't
use a custom megazord don't have to depend on application-services code.

It's separate from RustLog since it's plausible users might only want to
initialize logging, and not use any app-services network functionality.
