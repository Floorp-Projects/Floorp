use std::env;
use std::ffi::OsStr;
use std::ops::Drop;
use std::thread;

const STACK_ENV_VAR: &'static str = "RUST_MIN_STACK";
const EXTRA_STACK: &'static str = "16777216"; // 16 MB

/// Runs a function in a thread with extra stack space (16 MB or
/// `$RUST_MIN_STACK` if set).
///
/// The Rust parser uses a lot of stack space so codegen sometimes requires more
/// than is available by default.
///
/// ```rust,ignore
/// syntex::with_extra_stack(move || {
///     let mut reg = syntex::Registry::new();
///     reg.add_decorator(/* ... */);
///     reg.expand("", src, dst)
/// })
/// ```
///
/// This function runs with a 16 MB stack by default but a different value can
/// be set by the RUST_MIN_STACK environment variable.
pub fn with_extra_stack<F, T>(f: F) -> T
    where F: Send + 'static + FnOnce() -> T,
          T: Send + 'static
{
    let _tmp_env = set_if_unset(STACK_ENV_VAR, EXTRA_STACK);

    thread::spawn(f).join().unwrap()
}

fn set_if_unset<K, V>(k: K, v: V) -> TmpEnv<K>
    where K: AsRef<OsStr>,
          V: AsRef<OsStr>,
{
    match env::var(&k) {
        Ok(_) => TmpEnv::WasAlreadySet,
        Err(_) => {
            env::set_var(&k, v);
            TmpEnv::WasNotSet(k)
        }
    }
}

#[must_use]
enum TmpEnv<K>
    where K: AsRef<OsStr>,
{
    WasAlreadySet,
    WasNotSet(K),
}

impl<K> Drop for TmpEnv<K>
    where K: AsRef<OsStr>,
{
    fn drop(&mut self) {
        if let TmpEnv::WasNotSet(ref k) = *self {
            env::remove_var(k);
        }
    }
}
