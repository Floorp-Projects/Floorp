/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use md5::{Digest, Md5};
use url::{Host, Url};

pub type UrlHash = [u8; 16];

/// Given a URL, extract the part of it that we want to use to identify it.
///
/// We currently use the final 3 components of the URL domain.
///
/// TODO: decide if this should be 3 or 3 components.
pub fn url_hash_source(url: &str) -> Option<String> {
    let url = Url::parse(url).ok()?;
    let domain = match url.host() {
        Some(Host::Domain(d)) => d,
        _ => return None,
    };
    // This will store indexes of `.` chars as we search backwards.
    let mut pos = domain.len();
    for _ in 0..3 {
        match domain[0..pos].rfind('.') {
            Some(p) => pos = p,
            // The domain has less than 3 dots, return it all
            None => return Some(domain.to_owned()),
        }
    }
    Some(domain[pos + 1..].to_owned())
}

pub fn hash_url(url: &str) -> Option<UrlHash> {
    url_hash_source(url).map(|hash_source| {
        let mut hasher = Md5::new();
        hasher.update(hash_source);
        let result = hasher.finalize();
        result.into()
    })
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_url_hash_source() {
        let table = [
            ("http://example.com/some-path", Some("example.com")),
            ("http://foo.example.com/some-path", Some("foo.example.com")),
            (
                "http://foo.bar.baz.example.com/some-path",
                Some("baz.example.com"),
            ),
            ("http://foo.com.uk/some-path", Some("foo.com.uk")),
            ("http://amazon.com/some-path", Some("amazon.com")),
            ("http://192.168.0.1/some-path", None),
        ];
        for (url, expected) in table {
            assert_eq!(url_hash_source(url).as_deref(), expected)
        }
    }
}
