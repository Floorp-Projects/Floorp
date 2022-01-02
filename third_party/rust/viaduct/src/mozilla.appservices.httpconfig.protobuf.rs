#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Request {
    #[prost(enumeration="request::Method", required, tag="1")]
    pub method: i32,
    #[prost(string, required, tag="2")]
    pub url: std::string::String,
    #[prost(bytes, optional, tag="3")]
    pub body: ::std::option::Option<std::vec::Vec<u8>>,
    #[prost(map="string, string", tag="4")]
    pub headers: ::std::collections::HashMap<std::string::String, std::string::String>,
    #[prost(bool, required, tag="5")]
    pub follow_redirects: bool,
    #[prost(bool, required, tag="6")]
    pub use_caches: bool,
    #[prost(int32, required, tag="7")]
    pub connect_timeout_secs: i32,
    #[prost(int32, required, tag="8")]
    pub read_timeout_secs: i32,
}
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
    }
}
#[derive(Clone, PartialEq, ::prost::Message)]
pub struct Response {
    /// If this is present, nothing else is.
    #[prost(string, optional, tag="1")]
    pub exception_message: ::std::option::Option<std::string::String>,
    #[prost(string, optional, tag="2")]
    pub url: ::std::option::Option<std::string::String>,
    #[prost(int32, optional, tag="3")]
    pub status: ::std::option::Option<i32>,
    #[prost(bytes, optional, tag="4")]
    pub body: ::std::option::Option<std::vec::Vec<u8>>,
    #[prost(map="string, string", tag="5")]
    pub headers: ::std::collections::HashMap<std::string::String, std::string::String>,
}
