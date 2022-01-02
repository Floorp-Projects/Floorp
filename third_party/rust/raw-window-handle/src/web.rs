/// Raw window handle for the web
///
/// ## Construction
/// ```
/// # use raw_window_handle::web::WebHandle;
/// let handle = WebHandle {
///     /* fields */
///     ..WebHandle::empty()
/// };
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct WebHandle {
    /// An ID value inserted into the data attributes of the canvas element as 'raw-handle'
    ///
    /// When accessing from JS, the attribute will automatically be called rawHandle
    ///
    /// Each canvas created by the windowing system should be assigned their own unique ID.
    /// 0 should be reserved for invalid / null IDs.
    pub id: u32,
    #[doc(hidden)]
    #[deprecated = "This field is used to ensure that this struct is non-exhaustive, so that it may be extended in the future. Do not refer to this field."]
    pub _non_exhaustive_do_not_use: crate::seal::Seal,
}

impl WebHandle {
    pub fn empty() -> WebHandle {
        #[allow(deprecated)]
        WebHandle {
            id: 0,
            _non_exhaustive_do_not_use: crate::seal::Seal,
        }
    }
}
