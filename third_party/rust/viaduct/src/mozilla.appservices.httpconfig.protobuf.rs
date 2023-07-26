#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Request {
    #[prost(enumeration = "request::Method", required, tag = "1")]
    pub method: i32,
    #[prost(string, required, tag = "2")]
    pub url: ::prost::alloc::string::String,
    #[prost(bytes = "vec", optional, tag = "3")]
    pub body: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
    #[prost(map = "string, string", tag = "4")]
    pub headers: ::std::collections::HashMap<
        ::prost::alloc::string::String,
        ::prost::alloc::string::String,
    >,
    #[prost(bool, required, tag = "5")]
    pub follow_redirects: bool,
    #[prost(bool, required, tag = "6")]
    pub use_caches: bool,
    #[prost(int32, required, tag = "7")]
    pub connect_timeout_secs: i32,
    #[prost(int32, required, tag = "8")]
    pub read_timeout_secs: i32,
}
/// Nested message and enum types in `Request`.
pub mod request {
    #[derive(
        Clone,
        Copy,
        Debug,
        PartialEq,
        Eq,
        Hash,
        PartialOrd,
        Ord,
        ::prost::Enumeration
    )]
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
    impl Method {
        /// String value of the enum field names used in the ProtoBuf definition.
        ///
        /// The values are not transformed in any way and thus are considered stable
        /// (if the ProtoBuf definition does not change) and safe for programmatic use.
        pub fn as_str_name(&self) -> &'static str {
            match self {
                Method::Get => "GET",
                Method::Head => "HEAD",
                Method::Post => "POST",
                Method::Put => "PUT",
                Method::Delete => "DELETE",
                Method::Connect => "CONNECT",
                Method::Options => "OPTIONS",
                Method::Trace => "TRACE",
                Method::Patch => "PATCH",
            }
        }
        /// Creates an enum from field names used in the ProtoBuf definition.
        pub fn from_str_name(value: &str) -> ::core::option::Option<Self> {
            match value {
                "GET" => Some(Self::Get),
                "HEAD" => Some(Self::Head),
                "POST" => Some(Self::Post),
                "PUT" => Some(Self::Put),
                "DELETE" => Some(Self::Delete),
                "CONNECT" => Some(Self::Connect),
                "OPTIONS" => Some(Self::Options),
                "TRACE" => Some(Self::Trace),
                "PATCH" => Some(Self::Patch),
                _ => None,
            }
        }
    }
}
#[allow(clippy::derive_partial_eq_without_eq)]
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Response {
    /// If this is present, nothing else is.
    #[prost(string, optional, tag = "1")]
    pub exception_message: ::core::option::Option<::prost::alloc::string::String>,
    #[prost(string, optional, tag = "2")]
    pub url: ::core::option::Option<::prost::alloc::string::String>,
    #[prost(int32, optional, tag = "3")]
    pub status: ::core::option::Option<i32>,
    #[prost(bytes = "vec", optional, tag = "4")]
    pub body: ::core::option::Option<::prost::alloc::vec::Vec<u8>>,
    #[prost(map = "string, string", tag = "5")]
    pub headers: ::std::collections::HashMap<
        ::prost::alloc::string::String,
        ::prost::alloc::string::String,
    >,
}
