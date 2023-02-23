#![allow(dead_code)]

extern crate glob;
extern crate serial_test;
extern crate tempdir;

use std::collections::HashMap;
use std::env;
use std::fs;
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::Mutex;

use serial_test::serial;
use tempdir::TempDir;

#[macro_use]
#[path = "../build/macros.rs"]
mod macros;

#[path = "../build/common.rs"]
mod common;
#[path = "../build/dynamic.rs"]
mod dynamic;
#[path = "../build/static.rs"]
mod r#static;

#[derive(Debug, Default)]
struct RunCommandMock {
    invocations: Vec<(String, String, Vec<String>)>,
    responses: HashMap<Vec<String>, String>,
}

#[derive(Debug)]
struct Env {
    os: String,
    pointer_width: String,
    env: Option<String>,
    vars: HashMap<String, (Option<String>, Option<String>)>,
    cwd: PathBuf,
    tmp: TempDir,
    files: Vec<String>,
    commands: Arc<Mutex<RunCommandMock>>,
}

impl Env {
    fn new(os: &str, pointer_width: &str) -> Self {
        Env {
            os: os.into(),
            pointer_width: pointer_width.into(),
            env: None,
            vars: HashMap::new(),
            cwd: env::current_dir().unwrap(),
            tmp: TempDir::new("clang_sys_test").unwrap(),
            files: vec![],
            commands: Default::default(),
        }
        .var("CLANG_PATH", None)
        .var("LD_LIBRARY_PATH", None)
        .var("LIBCLANG_PATH", None)
        .var("LIBCLANG_STATIC_PATH", None)
        .var("LLVM_CONFIG_PATH", None)
        .var("PATH", None)
    }

    fn env(mut self, env: &str) -> Self {
        self.env = Some(env.into());
        self
    }

    fn var(mut self, name: &str, value: Option<&str>) -> Self {
        let previous = env::var(name).ok();
        self.vars.insert(name.into(), (value.map(|v| v.into()), previous));
        self
    }

    fn dir(mut self, path: &str) -> Self {
        self.files.push(path.into());
        let path = self.tmp.path().join(path);
        fs::create_dir_all(path).unwrap();
        self
    }

    fn file(mut self, path: &str, contents: &[u8]) -> Self {
        self.files.push(path.into());
        let path = self.tmp.path().join(path);
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(self.tmp.path().join(path), contents).unwrap();
        self
    }

    fn dll(self, path: &str, pointer_width: &str) -> Self {
        // PE header.
        let mut contents = [0; 64];
        contents[0x3C..0x3C + 4].copy_from_slice(&i32::to_le_bytes(10));
        contents[10..14].copy_from_slice(&[b'P', b'E', 0, 0]);
        let magic = if pointer_width == "64" { 523 } else { 267 };
        contents[34..36].copy_from_slice(&u16::to_le_bytes(magic));

        self.file(path, &contents)
    }

    fn so(self, path: &str, pointer_width: &str) -> Self {
        // ELF header.
        let class = if pointer_width == "64" { 2 } else { 1 };
        let contents = [127, 69, 76, 70, class];

        self.file(path, &contents)
    }

    fn command(self, command: &str, args: &[&str], response: &str) -> Self {
        let command = command.to_string();
        let args = args.iter().map(|a| a.to_string()).collect::<Vec<_>>();

        let mut key = vec![command];
        key.extend(args);
        self.commands.lock().unwrap().responses.insert(key, response.into());

        self
    }

    fn enable(self) -> Self {
        env::set_var("_CLANG_SYS_TEST", "yep");
        env::set_var("_CLANG_SYS_TEST_OS", &self.os);
        env::set_var("_CLANG_SYS_TEST_POINTER_WIDTH", &self.pointer_width);
        if let Some(env) = &self.env {
            env::set_var("_CLANG_SYS_TEST_ENV", env);
        }

        for (name, (value, _)) in &self.vars {
            if let Some(value) = value {
                env::set_var(name, value);
            } else {
                env::remove_var(name);
            }
        }

        env::set_current_dir(&self.tmp).unwrap();

        let commands = self.commands.clone();
        let mock = &mut *common::RUN_COMMAND_MOCK.lock().unwrap();
        *mock = Some(Box::new(move |command, path, args| {
            let command = command.to_string();
            let path = path.to_string();
            let args = args.iter().map(|a| a.to_string()).collect::<Vec<_>>();

            let mut commands = commands.lock().unwrap();
            commands.invocations.push((command.clone(), path, args.clone()));

            let mut key = vec![command];
            key.extend(args);
            commands.responses.get(&key).cloned()
        }));

        self
    }
}

impl Drop for Env {
    fn drop(&mut self) {
        env::remove_var("_CLANG_SYS_TEST");
        env::remove_var("_CLANG_SYS_TEST_OS");
        env::remove_var("_CLANG_SYS_TEST_POINTER_WIDTH");
        env::remove_var("_CLANG_SYS_TEST_ENV");

        for (name, (_, previous)) in &self.vars {
            if let Some(previous) = previous {
                env::set_var(name, previous);
            } else {
                env::remove_var(name);
            }
        }

        if let Err(error) = env::set_current_dir(&self.cwd) {
            println!("Failed to reset working directory: {:?}", error);
        }
    }
}

//================================================
// Dynamic
//================================================

// Linux -----------------------------------------

#[test]
#[serial]
fn test_linux_directory_preference() {
    let _env = Env::new("linux", "64")
        .so("usr/lib/libclang.so.1", "64")
        .so("usr/local/lib/libclang.so.1", "64")
        .enable();

    assert_eq!(
        dynamic::find(true),
        Ok(("usr/local/lib".into(), "libclang.so.1".into())),
    );
}

#[test]
#[serial]
fn test_linux_version_preference() {
    let _env = Env::new("linux", "64")
        .so("usr/lib/libclang-3.so", "64")
        .so("usr/lib/libclang-3.5.so", "64")
        .so("usr/lib/libclang-3.5.0.so", "64")
        .enable();

    assert_eq!(
        dynamic::find(true),
        Ok(("usr/lib".into(), "libclang-3.5.0.so".into())),
    );
}

#[test]
#[serial]
fn test_linux_directory_and_version_preference() {
    let _env = Env::new("linux", "64")
        .so("usr/local/llvm/lib/libclang-3.so", "64")
        .so("usr/local/lib/libclang-3.5.so", "64")
        .so("usr/lib/libclang-3.5.0.so", "64")
        .enable();

    assert_eq!(
        dynamic::find(true),
        Ok(("usr/lib".into(), "libclang-3.5.0.so".into())),
    );
}

// Windows ---------------------------------------

#[cfg(target_os = "windows")]
#[test]
#[serial]
fn test_windows_bin_sibling() {
    let _env = Env::new("windows", "64")
        .dir("Program Files\\LLVM\\lib")
        .dll("Program Files\\LLVM\\bin\\libclang.dll", "64")
        .enable();

    assert_eq!(
        dynamic::find(true),
        Ok(("Program Files\\LLVM\\bin".into(), "libclang.dll".into())),
    );
}

#[cfg(target_os = "windows")]
#[test]
#[serial]
fn test_windows_mingw_gnu() {
    let _env = Env::new("windows", "64")
        .env("gnu")
        .dir("MSYS\\MinGW\\lib")
        .dll("MSYS\\MinGW\\bin\\clang.dll", "64")
        .dir("Program Files\\LLVM\\lib")
        .dll("Program Files\\LLVM\\bin\\libclang.dll", "64")
        .enable();

    assert_eq!(
        dynamic::find(true),
        Ok(("MSYS\\MinGW\\bin".into(), "clang.dll".into())),
    );
}

#[cfg(target_os = "windows")]
#[test]
#[serial]
fn test_windows_mingw_msvc() {
    let _env = Env::new("windows", "64")
        .env("msvc")
        .dir("MSYS\\MinGW\\lib")
        .dll("MSYS\\MinGW\\bin\\clang.dll", "64")
        .dir("Program Files\\LLVM\\lib")
        .dll("Program Files\\LLVM\\bin\\libclang.dll", "64")
        .enable();

    assert_eq!(
        dynamic::find(true),
        Ok(("Program Files\\LLVM\\bin".into(), "libclang.dll".into())),
    );
}
