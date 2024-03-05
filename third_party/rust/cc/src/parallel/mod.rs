pub(crate) mod async_executor;
pub(crate) mod job_token;
pub(crate) mod stderr;

/// Remove all element in `vec` which `f(element)` returns `false`.
///
/// TODO: Remove this once the MSRV is bumped to v1.61
pub(crate) fn retain_unordered_mut<T, F>(vec: &mut Vec<T>, mut f: F)
where
    F: FnMut(&mut T) -> bool,
{
    let mut i = 0;
    while i < vec.len() {
        if f(&mut vec[i]) {
            i += 1;
        } else {
            vec.swap_remove(i);
        }
    }
}
