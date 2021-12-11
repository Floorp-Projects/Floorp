/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        && request.url.host_str() != Some("localhost")
        && request.url.host_str() != Some("127.0.0.1")
    {
        return Err(crate::Error::NonTlsUrl);
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::validate_request;
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
    }
}
