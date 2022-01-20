use std::convert::TryInto;

use crate::vk;
pub type VkResult<T> = Result<T, vk::Result>;

impl From<vk::Result> for VkResult<()> {
    fn from(err_code: vk::Result) -> Self {
        err_code.result()
    }
}

impl vk::Result {
    pub fn result(self) -> VkResult<()> {
        self.result_with_success(())
    }

    pub fn result_with_success<T>(self, v: T) -> VkResult<T> {
        match self {
            vk::Result::SUCCESS => Ok(v),
            _ => Err(self),
        }
    }
}

/// Repeatedly calls `f` until it does not return [`vk::Result::INCOMPLETE`] anymore,
/// ensuring all available data has been read into the vector.
///
/// See for example [`vkEnumerateInstanceExtensionProperties`]: the number of available
/// items may change between calls; [`vk::Result::INCOMPLETE`] is returned when the count
/// increased (and the vector is not large enough after querying the initial size),
/// requiring Ash to try again.
///
/// [`vkEnumerateInstanceExtensionProperties`]: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceExtensionProperties.html
pub(crate) unsafe fn read_into_uninitialized_vector<N: Copy + Default + TryInto<usize>, T>(
    f: impl Fn(&mut N, *mut T) -> vk::Result,
) -> VkResult<Vec<T>>
where
    <N as TryInto<usize>>::Error: std::fmt::Debug,
{
    loop {
        let mut count = N::default();
        f(&mut count, std::ptr::null_mut()).result()?;
        let mut data =
            Vec::with_capacity(count.try_into().expect("`N` failed to convert to `usize`"));

        let err_code = f(&mut count, data.as_mut_ptr());
        if err_code != vk::Result::INCOMPLETE {
            data.set_len(count.try_into().expect("`N` failed to convert to `usize`"));
            break err_code.result_with_success(data);
        }
    }
}

/// Repeatedly calls `f` until it does not return [`vk::Result::INCOMPLETE`] anymore,
/// ensuring all available data has been read into the vector.
///
/// Items in the target vector are [`default()`][`Default::default()`]-initialized which
/// is required for [`vk::BaseOutStructure`]-like structs where [`vk::BaseOutStructure::s_type`]
/// needs to be a valid type and [`vk::BaseOutStructure::p_next`] a valid or
/// [`null`][`std::ptr::null_mut()`] pointer.
///
/// See for example [`vkEnumerateInstanceExtensionProperties`]: the number of available
/// items may change between calls; [`vk::Result::INCOMPLETE`] is returned when the count
/// increased (and the vector is not large enough after querying the initial size),
/// requiring Ash to try again.
///
/// [`vkEnumerateInstanceExtensionProperties`]: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkEnumerateInstanceExtensionProperties.html
pub(crate) unsafe fn read_into_defaulted_vector<
    N: Copy + Default + TryInto<usize>,
    T: Default + Clone,
>(
    f: impl Fn(&mut N, *mut T) -> vk::Result,
) -> VkResult<Vec<T>>
where
    <N as TryInto<usize>>::Error: std::fmt::Debug,
{
    loop {
        let mut count = N::default();
        f(&mut count, std::ptr::null_mut()).result()?;
        let mut data =
            vec![Default::default(); count.try_into().expect("`N` failed to convert to `usize`")];

        let err_code = f(&mut count, data.as_mut_ptr());
        if err_code != vk::Result::INCOMPLETE {
            data.set_len(count.try_into().expect("`N` failed to convert to `usize`"));
            break err_code.result_with_success(data);
        }
    }
}
