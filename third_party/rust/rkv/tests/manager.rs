// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::fs;
use std::sync::Arc;

use tempfile::Builder;

use rkv::backend::{
    Lmdb,
    LmdbEnvironment,
    SafeMode,
    SafeModeEnvironment,
};
use rkv::Rkv;

/// Test that a manager can be created with simple type inference.
#[test]
fn test_simple() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let _ = Manager::singleton().write().unwrap();
}

/// Test that a manager can be created with simple type inference.
#[test]
fn test_simple_safe() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let _ = Manager::singleton().write().unwrap();
}

/// Test that a shared Rkv instance can be created with simple type inference.
#[test]
fn test_simple_2() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let root = Builder::new().prefix("test_simple_2").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();
    let _ = manager.get_or_create(root.path(), Rkv::new::<Lmdb>).unwrap();
}

/// Test that a shared Rkv instance can be created with simple type inference.
#[test]
fn test_simple_safe_2() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new().prefix("test_simple_safe_2").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();
    let _ = manager.get_or_create(root.path(), Rkv::new::<SafeMode>).unwrap();
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[test]
fn test_same() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let root = Builder::new().prefix("test_same").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let p = root.path();
    assert!(Manager::singleton().read().unwrap().get(p).expect("success").is_none());

    let created_arc = Manager::singleton().write().unwrap().get_or_create(p, Rkv::new::<Lmdb>).expect("created");
    let fetched_arc = Manager::singleton().read().unwrap().get(p).expect("success").expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[test]
fn test_same_safe() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new().prefix("test_same_safe").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let p = root.path();
    assert!(Manager::singleton().read().unwrap().get(p).expect("success").is_none());

    let created_arc = Manager::singleton().write().unwrap().get_or_create(p, Rkv::new::<SafeMode>).expect("created");
    let fetched_arc = Manager::singleton().read().unwrap().get(p).expect("success").expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[test]
fn test_same_with_capacity() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let root = Builder::new().prefix("test_same_with_capacity").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();

    let p = root.path();
    assert!(manager.get(p).expect("success").is_none());

    let created_arc = manager.get_or_create_with_capacity(p, 10, Rkv::with_capacity::<Lmdb>).expect("created");
    let fetched_arc = manager.get(p).expect("success").expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[test]
fn test_same_with_capacity_safe() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new().prefix("test_same_with_capacity_safe").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();

    let p = root.path();
    assert!(manager.get(p).expect("success").is_none());

    let created_arc = manager.get_or_create_with_capacity(p, 10, Rkv::with_capacity::<SafeMode>).expect("created");
    let fetched_arc = manager.get(p).expect("success").expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}
