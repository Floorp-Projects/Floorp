pub mod prelude {
    pub use super::Handle;
}

pub trait Handle<T> {
    fn as_ptr(&self) -> *mut T;
}
