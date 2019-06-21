use {HeaderValue};
use super::ETag;
use util::FlatCsv;

/// `If-Match` header, defined in
/// [RFC7232](https://tools.ietf.org/html/rfc7232#section-3.1)
///
/// The `If-Match` header field makes the request method conditional on
/// the recipient origin server either having at least one current
/// representation of the target resource, when the field-value is "*",
/// or having a current representation of the target resource that has an
/// entity-tag matching a member of the list of entity-tags provided in
/// the field-value.
///
/// An origin server MUST use the strong comparison function when
/// comparing entity-tags for `If-Match`, since the client
/// intends this precondition to prevent the method from being applied if
/// there have been any changes to the representation data.
///
/// # ABNF
///
/// ```text
/// If-Match = "*" / 1#entity-tag
/// ```
///
/// # Example values
///
/// * `"xyzzy"`
/// * "xyzzy", "r2d2xxxx", "c3piozzzz"
///
/// # Examples
///
/// ```
/// # extern crate headers;
/// use headers::IfMatch;
///
/// let if_match = IfMatch::any();
/// ```
#[derive(Clone, Debug, PartialEq, Header)]
pub struct IfMatch(FlatCsv);

impl IfMatch {
    /// Create a new `If-Match: *` header.
    pub fn any() -> IfMatch {
        IfMatch(HeaderValue::from_static("*").into())
    }
}

impl From<ETag> for IfMatch {
    fn from(etag: ETag) -> IfMatch {
        IfMatch(HeaderValue::from(etag.0).into())
    }
}

