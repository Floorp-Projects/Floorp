use core::mem::ManuallyDrop;

#[repr(C)]
union DiplomatResultValue<T, E> {
    ok: ManuallyDrop<T>,
    err: ManuallyDrop<E>,
}

/// A `Result`-like type that can be passed across the FFI boundary
/// as a value. Use this type when returning a result from an FFI
/// function instead of `Result`.
#[repr(C)]
pub struct DiplomatResult<T, E> {
    value: DiplomatResultValue<T, E>,
    pub is_ok: bool,
}

impl<T, E> Drop for DiplomatResult<T, E> {
    fn drop(&mut self) {
        unsafe {
            if self.is_ok {
                let _ = ManuallyDrop::take(&mut self.value.ok);
            } else {
                let _ = ManuallyDrop::take(&mut self.value.err);
            }
        }
    }
}

impl<T, E> From<Result<T, E>> for DiplomatResult<T, E> {
    fn from(result: Result<T, E>) -> Self {
        match result {
            Result::Ok(ok) => DiplomatResult {
                value: DiplomatResultValue {
                    ok: ManuallyDrop::new(ok),
                },
                is_ok: true,
            },

            Result::Err(err) => DiplomatResult {
                value: DiplomatResultValue {
                    err: ManuallyDrop::new(err),
                },
                is_ok: false,
            },
        }
    }
}

impl<T, E> From<DiplomatResult<T, E>> for Result<T, E> {
    fn from(mut result: DiplomatResult<T, E>) -> Result<T, E> {
        unsafe {
            if result.is_ok {
                Ok(ManuallyDrop::take(&mut result.value.ok))
            } else {
                Err(ManuallyDrop::take(&mut result.value.err))
            }
        }
    }
}
