/// Raw display handle for the Web.
///
/// ## Construction
/// ```
/// # use raw_window_handle::WebDisplayHandle;
/// let mut display_handle = WebDisplayHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WebDisplayHandle;

impl WebDisplayHandle {
    pub fn empty() -> Self {
        Self {}
    }
}

/// Raw window handle for the Web.
///
/// ## Construction
/// ```
/// # use raw_window_handle::WebWindowHandle;
/// let mut window_handle = WebWindowHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WebWindowHandle {
    /// An ID value inserted into the [data attributes] of the canvas element as '`raw-handle`'.
    ///
    /// When accessing from JS, the attribute will automatically be called `rawHandle`.
    ///
    /// Each canvas created by the windowing system should be assigned their own unique ID.
    /// 0 should be reserved for invalid / null IDs.
    ///
    /// [data attributes]: https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/data-*
    pub id: u32,
}

impl WebWindowHandle {
    pub fn empty() -> Self {
        Self { id: 0 }
    }
}
