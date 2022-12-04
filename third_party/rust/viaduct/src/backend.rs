/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::GLOBAL_SETTINGS;
use ffi::FfiBackend;
use once_cell::sync::OnceCell;
mod ffi;

pub fn note_backend(which: &str) {
    // If trace logs are enabled: log on every request. Otherwise, just log on
    // the first request at `info` level. We remember if the Once was triggered
    // to avoid logging twice in the first case.
    static NOTE_BACKEND_ONCE: std::sync::Once = std::sync::Once::new();
    let mut called = false;
    NOTE_BACKEND_ONCE.call_once(|| {
        log::info!("Using HTTP backend {}", which);
        called = true;
    });
    if !called {
        log::trace!("Using HTTP backend {}", which);
    }
}

pub trait Backend: Send + Sync + 'static {
    fn send(&self, request: crate::Request) -> Result<crate::Response, crate::Error>;
}

static BACKEND: OnceCell<&'static dyn Backend> = OnceCell::new();

pub fn set_backend(b: &'static dyn Backend) -> Result<(), crate::Error> {
    BACKEND
        .set(b)
        .map_err(|_| crate::error::Error::SetBackendError)
}

pub(crate) fn get_backend() -> &'static dyn Backend {
    *BACKEND.get_or_init(|| Box::leak(Box::new(FfiBackend)))
}

pub fn send(request: crate::Request) -> Result<crate::Response, crate::Error> {
    validate_request(&request)?;
    get_backend().send(request)
}

pub fn validate_request(request: &crate::Request) -> Result<(), crate::Error> {
    if request.url.scheme() != "https"
        && match request.url.host() {
            Some(url::Host::Domain(d)) => d != "localhost",
            Some(url::Host::Ipv4(addr)) => !addr.is_loopback(),
            Some(url::Host::Ipv6(addr)) => !addr.is_loopback(),
            None => true,
        }
        && {
            let settings = GLOBAL_SETTINGS.read();
            settings
                .addn_allowed_insecure_url
                .as_ref()
                .map(|url| url.host() != request.url.host() || url.scheme() != request.url.scheme())
                .unwrap_or(true)
        }
    {
        return Err(crate::Error::NonTlsUrl);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validate_request() {
        let _https_request = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("https://www.example.com").unwrap(),
        );
        assert!(validate_request(&_https_request).is_ok());

        let _http_request = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("http://www.example.com").unwrap(),
        );
        assert!(validate_request(&_http_request).is_err());

        let _localhost_https_request = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("https://127.0.0.1/index.html").unwrap(),
        );
        assert!(validate_request(&_localhost_https_request).is_ok());

        let _localhost_https_request_2 = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("https://localhost:4242/").unwrap(),
        );
        assert!(validate_request(&_localhost_https_request_2).is_ok());

        let _localhost_http_request = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("http://localhost:4242/").unwrap(),
        );
        assert!(validate_request(&_localhost_http_request).is_ok());

        let localhost_request = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("localhost:4242/").unwrap(),
        );
        assert!(validate_request(&localhost_request).is_err());

        let localhost_request_shorthand_ipv6 =
            crate::Request::new(crate::Method::Get, url::Url::parse("http://[::1]").unwrap());
        assert!(validate_request(&localhost_request_shorthand_ipv6).is_ok());

        let localhost_request_ipv6 = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("http://[0:0:0:0:0:0:0:1]").unwrap(),
        );
        assert!(validate_request(&localhost_request_ipv6).is_ok());
    }

    #[test]
    fn test_validate_request_addn_allowed_insecure_url() {
        let request_root = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("http://anything").unwrap(),
        );
        let request = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("http://anything/path").unwrap(),
        );
        // This should never be accepted.
        let request_ftp = crate::Request::new(
            crate::Method::Get,
            url::Url::parse("ftp://anything/path").unwrap(),
        );
        assert!(validate_request(&request_root).is_err());
        assert!(validate_request(&request).is_err());
        {
            let mut settings = GLOBAL_SETTINGS.write();
            settings.addn_allowed_insecure_url =
                Some(url::Url::parse("http://something-else").unwrap());
        }
        assert!(validate_request(&request_root).is_err());
        assert!(validate_request(&request).is_err());

        {
            let mut settings = GLOBAL_SETTINGS.write();
            settings.addn_allowed_insecure_url = Some(url::Url::parse("http://anything").unwrap());
        }
        assert!(validate_request(&request_root).is_ok());
        assert!(validate_request(&request).is_ok());
        assert!(validate_request(&request_ftp).is_err());
    }
}
