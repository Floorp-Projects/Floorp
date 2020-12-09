// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// TODO: Allow argument conversions, so that implementations don't need to do the boilerplate
// themselves.

/// Define the global handle map, constructor and destructor functions and any user-defined
/// functions for a new metric
///
/// This allows to define most common functionality and simple operations for a metric type.
/// More complex operations should be written as plain functions directly.
///
/// # Arguments
///
/// * `$metric_map` - name of the map in `metrics::__glean_metric_maps` to fetch metrics from, should be all uppercase, e.g. `COUNTERS`.
/// * `$ffi_has_fn` - FFI name of the `test_has_value` function, something like `fog_counter_test_has_value`.
/// * `$ffi_get_fn` - FFI name of the `test_get_value` function, something like `fog_counter_test_get_value`.
/// * `get_ret` - Return value of the `test_get_value` function.
///
/// Additional simple functions can be defined as a mapping `$op -> $ffi_fn`:
///
/// * `$op` - function on the metric type to call.
/// * `$op_fn` - FFI function name for the operation, followed by its arguments and the return type.
///              Arguments are passed on as is.
///              If arguments need a conversion before being passed to the metric implementation
///              the FFI function needs to be implemented manually.
macro_rules! define_metric_ffi {
    ($metric_map:ident {
        test_has -> $ffi_has_fn:ident,
        test_get -> $ffi_get_fn:ident: $get_ret:ty,

        $(
            $op:ident -> $ffi_fn:ident($($op_argname:ident: $op_argtyp:ty),* $(,)*)
        ),* $(,)*
    }) => {
        $(
        #[no_mangle]
        pub extern "C" fn $ffi_fn(id: u32, $($op_argname: $op_argtyp),*) {
            match $crate::metrics::__glean_metric_maps::$metric_map.get(&id.into()) {
                Some(metric) => metric.$op($($op_argname),*),
                None => panic!("No metric for id {}", id),
            }
        }
        )*

        /// FFI wrapper to test whether a value is stored for the metric.
        ///
        /// Returns `1` if a metric value exists, otherwise `0`.
        ///
        /// Panics if no metric exists for the given ID.
        /// This indicates a bug in the calling code, as this method is only called through generated code,
        /// which will guarantee the use of  valid IDs.
        #[no_mangle]
        pub unsafe extern "C" fn $ffi_has_fn(id: u32, storage_name: *const ::std::os::raw::c_char) -> u8 {
            let storage_name = match ::std::ffi::CStr::from_ptr(storage_name).to_str() {
                Ok(s) => s,
                Err(e) => panic!("Invalid string for storage name, metric id {}, error: {:?}", id, e),
            };
            match $crate::metrics::__glean_metric_maps::$metric_map.get(&id.into()) {
                Some(metric) => metric.test_get_value(storage_name).is_some() as u8,
                None => panic!("No metric for id {}", id)
            }
        }

        /// FFI wrapper to get the currently stored value.
        ///
        /// Returns the value of the stored metric.
        ///
        /// Panics if no metric exists for the given ID.
        /// This indicates a bug in the calling code, as this method is only called through generated code,
        /// which will guarantee the use of  valid IDs.
        #[no_mangle]
        pub unsafe extern "C" fn $ffi_get_fn(id: u32, storage_name: *const ::std::os::raw::c_char) -> $get_ret {
            let storage_name = match ::std::ffi::CStr::from_ptr(storage_name).to_str() {
                Ok(s) => s,
                Err(e) => panic!("Invalid string for storage name, metric id {}, error: {:?}", id, e),
            };
            match $crate::metrics::__glean_metric_maps::$metric_map.get(&id.into()) {
                Some(metric) => metric.test_get_value(storage_name).unwrap().into(),
                None => panic!("No metric for id {}", id)
            }
        }
    }
}
