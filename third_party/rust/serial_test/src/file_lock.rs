use fslock::LockFile;
use std::{env, fs, path::Path};

struct Lock {
    lockfile: LockFile,
}

impl Lock {
    fn unlock(self: &mut Lock) {
        self.lockfile.unlock().unwrap();
        println!("Unlock");
    }
}

fn do_lock(path: &str) -> Lock {
    if !Path::new(path).exists() {
        fs::write(path, "").unwrap_or_else(|_| panic!("Lock file path was {:?}", path))
    }
    let mut lockfile = LockFile::open(path).unwrap();
    println!("Waiting on {:?}", path);
    lockfile.lock().unwrap();
    println!("Locked for {:?}", path);
    Lock { lockfile }
}

fn path_for_name(name: &str) -> String {
    let mut pathbuf = env::temp_dir();
    pathbuf.push(format!("serial-test-{}", name));
    pathbuf.into_os_string().into_string().unwrap()
}

fn make_lock_for_name_and_path(name: &str, path: Option<&str>) -> Lock {
    if let Some(opt_path) = path {
        do_lock(opt_path)
    } else {
        let default_path = path_for_name(name);
        do_lock(&default_path)
    }
}

#[doc(hidden)]
pub fn fs_serial_core(name: &str, path: Option<&str>, function: fn()) {
    let mut lock = make_lock_for_name_and_path(name, path);
    function();
    lock.unlock();
}

#[doc(hidden)]
pub fn fs_serial_core_with_return<E>(
    name: &str,
    path: Option<&str>,
    function: fn() -> Result<(), E>,
) -> Result<(), E> {
    let mut lock = make_lock_for_name_and_path(name, path);
    let ret = function();
    lock.unlock();
    ret
}

#[doc(hidden)]
pub async fn fs_async_serial_core_with_return<E>(
    name: &str,
    path: Option<&str>,
    fut: impl std::future::Future<Output = Result<(), E>>,
) -> Result<(), E> {
    let mut lock = make_lock_for_name_and_path(name, path);
    let ret = fut.await;
    lock.unlock();
    ret
}

#[doc(hidden)]
pub async fn fs_async_serial_core(
    name: &str,
    path: Option<&str>,
    fut: impl std::future::Future<Output = ()>,
) {
    let mut lock = make_lock_for_name_and_path(name, path);
    fut.await;
    lock.unlock();
}

#[test]
fn test_serial() {
    fs_serial_core("test", None, || {});
}
