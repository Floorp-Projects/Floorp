/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Gecko profiler label support.
//!
//! Use the `profiler_label!` macro directly instead of using `AutoProfilerLabel`.
//! See the `profiler_label!` macro documentation on how to use it.

#[cfg(feature = "enabled")]
use crate::gecko_bindings::{
    bindings, profiling_categories::ProfilingCategoryPair, structs::mozilla,
};

/// RAII object that constructs and destroys a C++ AutoProfilerLabel object
/// pointed to be the specified reference.
/// Use `profiler_label!` macro directly instead of this, if possible.
#[cfg(feature = "enabled")]
pub struct AutoProfilerLabel<'a>(&'a mut mozilla::AutoProfilerLabel);

#[cfg(feature = "enabled")]
impl<'a> AutoProfilerLabel<'a> {
    /// Creates a new AutoProfilerLabel with the specified label type.
    ///
    /// unsafe since the caller must ensure that `label` is allocated on the
    /// stack.
    #[inline]
    pub unsafe fn new(
        label: &mut std::mem::MaybeUninit<mozilla::AutoProfilerLabel>,
        category_pair: ProfilingCategoryPair,
    ) -> AutoProfilerLabel {
        bindings::gecko_profiler_construct_label(
            label.as_mut_ptr(),
            category_pair.to_cpp_enum_value(),
        );
        AutoProfilerLabel(&mut *label.as_mut_ptr())
    }
}

#[cfg(feature = "enabled")]
impl<'a> Drop for AutoProfilerLabel<'a> {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            bindings::gecko_profiler_destruct_label(self.0);
        }
    }
}

/// Place a Gecko profiler label on the stack.
///
/// The first `category` argument must be the name of a variant of `ProfilerLabelCategoryPair`
/// and the second optional `subcategory` argument must be one of the sub variants of
/// `ProfilerLabelCategoryPair`. All options can be seen either in the
/// profiling_categories.yaml file or generated profiling_categories.rs file.
///
/// Example usage:
/// ```rust
/// gecko_profiler_label!(Layout);
/// gecko_profiler_label!(JavaScript, Parsing);
/// ```
/// You can wrap this macro with a block to only label a specific part of a function.
#[cfg(feature = "enabled")]
#[macro_export]
macro_rules! gecko_profiler_label {
    ($category:ident) => {
        gecko_profiler_label!($crate::ProfilingCategoryPair::$category(None))
    };
    ($category:ident, $subcategory:ident) => {
        gecko_profiler_label!($crate::ProfilingCategoryPair::$category(Some(
            $crate::$category::$subcategory
        )))
    };

    ($category_path:expr) => {
        let mut _profiler_label = ::std::mem::MaybeUninit::<
            $crate::gecko_bindings::structs::mozilla::AutoProfilerLabel,
        >::uninit();
        let _profiler_label = if $crate::is_active() {
            unsafe {
                Some($crate::AutoProfilerLabel::new(
                    &mut _profiler_label,
                    $category_path,
                ))
            }
        } else {
            None
        };
    };
}

/// No-op when MOZ_GECKO_PROFILER is not defined.
#[cfg(not(feature = "enabled"))]
#[macro_export]
macro_rules! gecko_profiler_label {
    ($category:ident) => {};
    ($category:ident, $subcategory:ident) => {};
}

#[cfg(test)]
mod tests {
    use profiler_macros::gecko_profiler_fn_label;

    #[test]
    fn test_gecko_profiler_label() {
        gecko_profiler_label!(Layout);
        gecko_profiler_label!(JavaScript, Parsing);
    }

    #[gecko_profiler_fn_label(DOM)]
    fn foo(bar: u32) -> u32 {
        bar
    }

    #[gecko_profiler_fn_label(Javascript, IonMonkey)]
    pub(self) fn bar(baz: i8) -> i8 {
        baz
    }

    struct A;

    impl A {
        #[gecko_profiler_fn_label(Idle)]
        pub fn test(&self) -> i8 {
            1
        }
    }

    #[test]
    fn test_gecko_profiler_fn_label() {
        let _: u32 = foo(100000);
        let _: i8 = bar(127);

        let a = A;
        let _ = a.test(100);
    }
}
