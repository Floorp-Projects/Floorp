//! Utility functions, only pathname handling at the moment.

pub fn basename(f: &str) -> &str {
    match f.rfind(|c| c == '/' || c == '\\') {
        None => f,
        Some(index) => &f[(index + 1)..],
    }
}
