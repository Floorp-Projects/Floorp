#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Request {
    #[prost(enumeration="request::Method", required, tag="1")]
    pub method: i32,
    #[prost(string, required, tag="2")]
    pub url: ::prost::alloc::string::String,
    #[prost(bytes="vec", optional, tag="3")]
    pub body: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
    #[prost(map="string, string", tag="4")]
    pub headers: ::std::collections::HashMap<::prost::alloc::string::String, ::prost::alloc::string::String>,
    #[prost(bool, required, tag="5")]
    pub follow_redirects: bool,
    #[prost(bool, required, tag="6")]
    pub use_caches: bool,
    #[prost(int32, required, tag="7")]
    pub connect_timeout_secs: i32,
    #[prost(int32, required, tag="8")]
    pub read_timeout_secs: i32,
}
/// Nested message and enum types in `Request`.
pub mod request {
    #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, ::prost::Enumeration)]
    #[repr(i32)]
    pub enum Method {
        Get = 0,
        Head = 1,
        Post = 2,
        Put = 3,
        Delete = 4,
        Connect = 5,
        Options = 6,
        Trace = 7,
        Patch = 8,
    }
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Response {
    /// If this is present, nothing else is.
    #[prost(string, optional, tag="1")]
    pub exception_message: ::core::option::Option<::prost::alloc::string::String>,
    #[prost(string, optional, tag="2")]
    pub url: ::core::option::Option<::prost::alloc::string::String>,
    #[prost(int32, optional, tag="3")]
    pub status: ::core::option::Option<i32>,
    #[prost(bytes="vec", optional, tag="4")]
    pub body: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
    #[prost(map="string, string", tag="5")]
    pub headers: ::std::collections::HashMap<::prost::alloc::string::String, ::prost::alloc::string::String>,
}
