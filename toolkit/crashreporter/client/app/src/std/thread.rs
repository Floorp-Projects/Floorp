/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use std::thread::*;

// Mock `spawn` just to issue a warning that mocking within the thread won't work without manual
// intervention.
pub fn spawn<F, T>(f: F) -> JoinHandle<T>
where
    F: FnOnce() -> T + Send + 'static,
    T: Send + 'static,
{
    eprintln!("warning: mocking won't work in `std::thread::spawn`ed threads by default. Use `std::mock::SharedMockData` if mocking is needed and it's safe to do so.");
    std::thread::spawn(f)
}

pub struct Scope<'scope, 'env: 'scope> {
    mock_data: super::mock::SharedMockData,
    scope: &'scope std::thread::Scope<'scope, 'env>,
}

impl<'scope, 'env> Scope<'scope, 'env> {
    pub fn spawn<F, T>(&self, f: F) -> ScopedJoinHandle<'scope, T>
    where
        F: FnOnce() -> T + Send + 'scope,
        T: Send + 'scope,
    {
        let mock_data = self.mock_data.clone();
        self.scope.spawn(move || {
            // # Safety
            // `thread::scope` guarantees that the mock data will outlive the thread.
            unsafe { mock_data.set() };
            f()
        })
    }
}

pub fn scope<'env, F, T>(f: F) -> T
where
    F: for<'scope> FnOnce(Scope<'scope, 'env>) -> T,
{
    let mock_data = super::mock::SharedMockData::new();
    std::thread::scope(|scope| f(Scope { mock_data, scope }))
}
