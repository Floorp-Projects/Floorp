/// Raw window handle for the Web.
///
/// ## Construction
/// ```
/// # use raw_window_handle::WebHandle;
/// let mut handle = WebHandle::empty();
/// /* set fields */
/// ```
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WebHandle {
    /// An ID value inserted into the data attributes of the canvas element as '`raw-handle`'.
    ///
    /// When accessing from JS, the attribute will automatically be called `rawHandle`.
    ///
    /// Each canvas created by the windowing system should be assigned their own unique ID.
    /// 0 should be reserved for invalid / null IDs.
    pub id: u32,
}

impl WebHandle {
    pub fn empty() -> Self {
        Self { id: 0 }
    }
}
