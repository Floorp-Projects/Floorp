#[macro_export]
macro_rules! define_infallible_handle_map_deleter {
    ($HANDLE_MAP_NAME:ident, $destructor_name:ident) => {
        #[no_mangle]
        pub extern "C" fn $destructor_name(v: u64) {
            let mut error = ffi_support::ExternError::success();
            let res = ffi_support::abort_on_panic::call_with_result(&mut error, || {
                let map: &$crate::ConcurrentHandleMap<_> = &*$HANDLE_MAP_NAME;
                map.delete_u64(v)
            });
            $crate::handlemap_ext::log_if_error(error);
            res
        }
    };
}
/// Define the global handle map, constructor and destructor functions and any user-defined
/// functions for a new metric
///
/// This allows to define most common functionality and simple operations for a metric type.
/// More complex operations should be written as plain functions directly.
///
/// # Arguments
///
/// * `$metric_type` - metric type to use from glean_core, e.g. `CounterMetric`.
/// * `$metric_map` - name to use for the global name, should be all uppercase, e.g. `COUNTER_METRICS`.
/// * `$new_fn(...)` - (optional) name of the constructor function, followed by all additional (non-common) arguments.
/// * `$test_get_num_recorded_errors` - (optional) name of the test_get_num_recorded_errors function
/// * `$destroy` - name of the destructor function.
///
/// Additional simple functions can be define as a mapping `$op -> $op_fn`:
///
/// * `$op` - function on the metric type to call.
/// * `$op_fn` - FFI function name for the operation, followed by its arguments.
///              Arguments are converted into the target type using `TryFrom::try_from`.
#[macro_export]
macro_rules! define_metric {
    ($metric_type:ident => $metric_map:ident {
        $(new -> $new_fn:ident($($new_argname:ident: $new_argtyp:ty),* $(,)*),)?
        $(test_get_num_recorded_errors -> $test_get_num_recorded_errors_fn:ident,)?
        destroy -> $destroy_fn:ident,

        $(
            $op:ident -> $op_fn:ident($($op_argname:ident: $op_argtyp:ty),* $(,)*)
        ),* $(,)*
    }) => {
        pub static $metric_map: once_cell::sync::Lazy<ffi_support::ConcurrentHandleMap<glean_core::metrics::$metric_type>> = once_cell::sync::Lazy::new(ffi_support::ConcurrentHandleMap::new);
        $crate::define_infallible_handle_map_deleter!($metric_map, $destroy_fn);

        $(
        #[no_mangle]
        pub extern "C" fn $new_fn(
            category: ffi_support::FfiStr,
            name: ffi_support::FfiStr,
            send_in_pings: crate::RawStringArray,
            send_in_pings_len: i32,
            lifetime: Lifetime,
            disabled: u8,
            $($new_argname: $new_argtyp),*
        ) -> u64 {
            $metric_map.insert_with_log(|| {
                let name = crate::FallibleToString::to_string_fallible(&name)?;
                let category = crate::FallibleToString::to_string_fallible(&category)?;
                let send_in_pings = crate::from_raw_string_array(send_in_pings, send_in_pings_len)?;
                let lifetime = std::convert::TryFrom::try_from(lifetime)?;

                $(
                    let $new_argname = std::convert::TryFrom::try_from($new_argname)?;
                )*

                Ok(glean_core::metrics::$metric_type::new(glean_core::CommonMetricData {
                    name,
                    category,
                    send_in_pings,
                    lifetime,
                    disabled: disabled != 0,
                    ..Default::default()
                }, $($new_argname),*))
            })
        }
        )?

        $(
        #[no_mangle]
        pub extern "C" fn $test_get_num_recorded_errors_fn(

            metric_id: u64,
            error_type: i32,
            storage_name: FfiStr
        ) -> i32 {
                crate::HandleMapExtension::call_infallible(&*$metric_map, metric_id, |metric| {
                    crate::with_glean_value(|glean| {
                        let error_type = std::convert::TryFrom::try_from(error_type).unwrap();
                        let storage_name = crate::FallibleToString::to_string_fallible(&storage_name).unwrap();
                        glean_core::test_get_num_recorded_errors(
                            &glean,
                            glean_core::metrics::MetricType::meta(metric),
                            error_type,
                            Some(&storage_name)
                        ).unwrap_or(0)
                    })
                })
        }
        )?

        $(
            #[no_mangle]
            pub extern "C" fn $op_fn( metric_id: u64, $($op_argname: $op_argtyp),*) {
                crate::with_glean_value(|glean| {
                    crate::HandleMapExtension::call_infallible(&*$metric_map, metric_id, |metric| {
                        metric.$op(&glean, $($op_argname),*);
                    })
                })
            }
        )*
    }
}
