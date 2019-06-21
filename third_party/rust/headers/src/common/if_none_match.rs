use util::{FlatCsv};
use super::ETag;
use {HeaderValue};

/// `If-None-Match` header, defined in
/// [RFC7232](https://tools.ietf.org/html/rfc7232#section-3.2)
///
/// The `If-None-Match` header field makes the request method conditional
/// on a recipient cache or origin server either not having any current
/// representation of the target resource, when the field-value is "*",
/// or having a selected representation with an entity-tag that does not
/// match any of those listed in the field-value.
///
/// A recipient MUST use the weak comparison function when comparing
/// entity-tags for If-None-Match (Section 2.3.2), since weak entity-tags
/// can be used for cache validation even if there have been changes to
/// the representation data.
///
/// # ABNF
///
/// ```text
/// If-None-Match = "*" / 1#entity-tag
/// ```
///
/// # Example values
///
/// * `"xyzzy"`
/// * `W/"xyzzy"`
/// * `"xyzzy", "r2d2xxxx", "c3piozzzz"`
/// * `W/"xyzzy", W/"r2d2xxxx", W/"c3piozzzz"`
/// * `*`
///
/// # Examples
///
/// ```
/// # extern crate headers;
/// use headers::IfNoneMatch;
///
/// let if_none_match = IfNoneMatch::any();
/// ```
#[derive(Clone, Debug, PartialEq, Header)]
pub struct IfNoneMatch(FlatCsv);

impl IfNoneMatch {
    /// Create a new `If-None-Match: *` header.
    pub fn any() -> IfNoneMatch {
        IfNoneMatch(HeaderValue::from_static("*").into())
    }
}

impl From<ETag> for IfNoneMatch {
    fn from(etag: ETag) -> IfNoneMatch {
        IfNoneMatch(HeaderValue::from(etag.0).into())
    }
}

    /*
    test_if_none_match {
        test_header!(test1, vec![b"\"xyzzy\""]);
        test_header!(test2, vec![b"W/\"xyzzy\""]);
        test_header!(test3, vec![b"\"xyzzy\", \"r2d2xxxx\", \"c3piozzzz\""]);
        test_header!(test4, vec![b"W/\"xyzzy\", W/\"r2d2xxxx\", W/\"c3piozzzz\""]);
        test_header!(test5, vec![b"*"]);
    }
    */

/*
#[cfg(test)]
mod tests {
    use super::IfNoneMatch;
    use Header;
    use EntityTag;

    #[test]
    fn test_if_none_match() {
        let mut if_none_match: ::Result<IfNoneMatch>;

        if_none_match = Header::parse_header(&b"*".as_ref().into());
        assert_eq!(if_none_match.ok(), Some(IfNoneMatch::Any));

        if_none_match = Header::parse_header(&b"\"foobar\", W/\"weak-etag\"".as_ref().into());
        let mut entities: Vec<EntityTag> = Vec::new();
        let foobar_etag = EntityTag::new(false, "foobar".to_owned());
        let weak_etag = EntityTag::new(true, "weak-etag".to_owned());
        entities.push(foobar_etag);
        entities.push(weak_etag);
        assert_eq!(if_none_match.ok(), Some(IfNoneMatch::Items(entities)));
    }
}
*/

