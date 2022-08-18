/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    dispatch_background_task_runnable, dispatch_runnable, get_current_thread, DispatchOptions,
};
use nserror::{nsresult, NS_OK};
use nsstring::nsACString;
use std::sync::Mutex;
use xpcom::interfaces::{nsIEventTarget, nsIRunnablePriority};
use xpcom::xpcom;

/// Basic wrapper to convert a FnOnce callback into a `nsIRunnable` to be
/// dispatched using XPCOM.
#[xpcom(implement(nsIRunnable, nsINamed, nsIRunnablePriority), atomic)]
struct RunnableFunction<F: FnOnce() + 'static> {
    name: &'static str,
    priority: u32,
    function: Mutex<Option<F>>,
}

impl<F: FnOnce() + 'static> RunnableFunction<F> {
    #[allow(non_snake_case)]
    fn Run(&self) -> nsresult {
        let function = self.function.lock().unwrap().take();
        debug_assert!(function.is_some(), "runnable invoked twice?");
        if let Some(function) = function {
            function();
        }
        NS_OK
    }

    #[allow(non_snake_case)]
    unsafe fn GetName(&self, result: *mut nsACString) -> nsresult {
        (*result).assign(self.name);
        NS_OK
    }

    #[allow(non_snake_case)]
    unsafe fn GetPriority(&self, result: *mut u32) -> nsresult {
        *result = self.priority;
        NS_OK
    }
}

pub struct RunnableBuilder<F> {
    name: &'static str,
    function: F,
    priority: u32,
    options: DispatchOptions,
}

impl<F> RunnableBuilder<F> {
    pub fn new(name: &'static str, function: F) -> Self {
        RunnableBuilder {
            name,
            function,
            priority: nsIRunnablePriority::PRIORITY_NORMAL,
            options: DispatchOptions::default(),
        }
    }

    pub fn priority(mut self, priority: u32) -> Self {
        self.priority = priority;
        self
    }

    pub fn options(mut self, options: DispatchOptions) -> Self {
        self.options = options;
        self
    }

    pub fn may_block(mut self, may_block: bool) -> Self {
        self.options = self.options.may_block(may_block);
        self
    }

    pub unsafe fn at_end(mut self, at_end: bool) -> Self {
        self.options = self.options.at_end(at_end);
        self
    }
}

impl<F> RunnableBuilder<F>
where
    F: FnOnce() + Send + 'static,
{
    /// Dispatch this Runnable to the specified EventTarget. The runnable function must be `Send`.
    pub fn dispatch(self, target: &nsIEventTarget) -> Result<(), nsresult> {
        let runnable = RunnableFunction::allocate(InitRunnableFunction {
            name: self.name,
            priority: self.priority,
            function: Mutex::new(Some(self.function)),
        });
        unsafe { dispatch_runnable(runnable.coerce(), target, self.options) }
    }

    /// Dispatch this Runnable to the specified EventTarget as a background
    /// task. The runnable function must be `Send`.
    pub fn dispatch_background_task(self) -> Result<(), nsresult> {
        let runnable = RunnableFunction::allocate(InitRunnableFunction {
            name: self.name,
            priority: self.priority,
            function: Mutex::new(Some(self.function)),
        });
        unsafe { dispatch_background_task_runnable(runnable.coerce(), self.options) }
    }
}

impl<F> RunnableBuilder<F>
where
    F: FnOnce() + 'static,
{
    /// Dispatch this Runnable to the current thread.
    ///
    /// Unlike `dispatch` and `dispatch_background_task`, the runnable does not
    /// need to be `Send` to dispatch to the current thread.
    pub fn dispatch_local(self) -> Result<(), nsresult> {
        let target = get_current_thread()?;
        let runnable = RunnableFunction::allocate(InitRunnableFunction {
            name: self.name,
            priority: self.priority,
            function: Mutex::new(Some(self.function)),
        });
        unsafe { dispatch_runnable(runnable.coerce(), target.coerce(), self.options) }
    }
}

pub fn dispatch_onto<F>(
    name: &'static str,
    target: &nsIEventTarget,
    function: F,
) -> Result<(), nsresult>
where
    F: FnOnce() + Send + 'static,
{
    RunnableBuilder::new(name, function).dispatch(target)
}

pub fn dispatch_background_task<F>(name: &'static str, function: F) -> Result<(), nsresult>
where
    F: FnOnce() + Send + 'static,
{
    RunnableBuilder::new(name, function).dispatch_background_task()
}

pub fn dispatch_local<F>(name: &'static str, function: F) -> Result<(), nsresult>
where
    F: FnOnce() + 'static,
{
    RunnableBuilder::new(name, function).dispatch_local()
}
