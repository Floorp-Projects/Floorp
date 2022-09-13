/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate url;
use url::quirks;
use url::{ParseOptions, Position, Url};

extern crate nsstring;
use nsstring::{nsACString, nsCString};

extern crate nserror;
use nserror::*;

extern crate xpcom;
use xpcom::interfaces::{mozIThirdPartyUtil, nsrefcnt};
use xpcom::{AtomicRefcnt, RefCounted, RefPtr};

extern crate uuid;
use uuid::Uuid;

use std::fmt::Write;
use std::marker::PhantomData;
use std::mem;
use std::ops;
use std::ptr;
use std::str;

extern "C" {
    fn Gecko_StrictFileOriginPolicy() -> bool;
}

/// Helper macro. If the expression $e is Ok(t) evaluates to t, otherwise,
/// returns NS_ERROR_MALFORMED_URI.
macro_rules! try_or_malformed {
    ($e:expr) => {
        match $e {
            Ok(v) => v,
            Err(_) => return NS_ERROR_MALFORMED_URI,
        }
    };
}

fn parser<'a>() -> ParseOptions<'a> {
    Url::options()
}

fn default_port(scheme: &str) -> Option<u16> {
    match scheme {
        "ftp" => Some(21),
        "gopher" => Some(70),
        "http" => Some(80),
        "https" => Some(443),
        "ws" => Some(80),
        "wss" => Some(443),
        "rtsp" => Some(443),
        "moz-anno" => Some(443),
        "android" => Some(443),
        _ => None,
    }
}

/// A slice into the backing string. This type is only valid as long as the
/// MozURL which it was pulled from is valid. In C++, this type implicitly
/// converts to a nsDependentCString, and is an implementation detail.
///
/// This type exists because, unlike &str, this type is safe to return over FFI.
#[repr(C)]
pub struct SpecSlice<'a> {
    data: *const u8,
    len: u32,
    _marker: PhantomData<&'a [u8]>,
}

impl<'a> From<&'a str> for SpecSlice<'a> {
    fn from(s: &'a str) -> SpecSlice<'a> {
        assert!(s.len() < u32::max_value() as usize);
        SpecSlice {
            data: s.as_ptr(),
            len: s.len() as u32,
            _marker: PhantomData,
        }
    }
}

/// The MozURL reference-counted threadsafe URL type. This type intentionally
/// implements no XPCOM interfaces, and all method calls are non-virtual.
#[repr(C)]
pub struct MozURL {
    pub url: Url,
    refcnt: AtomicRefcnt,
}

impl MozURL {
    pub fn from_url(url: Url) -> RefPtr<MozURL> {
        // Actually allocate the URL on the heap. This is the only place we actually
        // create a MozURL, other than in clone().
        unsafe {
            RefPtr::from_raw(Box::into_raw(Box::new(MozURL {
                url: url,
                refcnt: AtomicRefcnt::new(),
            })))
            .unwrap()
        }
    }
}

impl ops::Deref for MozURL {
    type Target = Url;
    fn deref(&self) -> &Url {
        &self.url
    }
}
impl ops::DerefMut for MozURL {
    fn deref_mut(&mut self) -> &mut Url {
        &mut self.url
    }
}

// Memory Management for MozURL
#[no_mangle]
pub unsafe extern "C" fn mozurl_addref(url: &MozURL) -> nsrefcnt {
    url.refcnt.inc()
}

#[no_mangle]
pub unsafe extern "C" fn mozurl_release(url: &MozURL) -> nsrefcnt {
    let rc = url.refcnt.dec();
    if rc == 0 {
        mem::drop(Box::from_raw(url as *const MozURL as *mut MozURL));
    }
    rc
}

// xpcom::RefPtr support
unsafe impl RefCounted for MozURL {
    unsafe fn addref(&self) {
        mozurl_addref(self);
    }
    unsafe fn release(&self) {
        mozurl_release(self);
    }
}

// Allocate a new MozURL object with a RefCnt of 1, and store a pointer to it
// into url.
#[no_mangle]
pub extern "C" fn mozurl_new(
    result: &mut *const MozURL,
    spec: &nsACString,
    base: Option<&MozURL>,
) -> nsresult {
    *result = ptr::null_mut();

    let spec = try_or_malformed!(str::from_utf8(spec));
    let url = if let Some(base) = base {
        try_or_malformed!(base.url.join(spec))
    } else {
        try_or_malformed!(parser().parse(spec))
    };

    MozURL::from_url(url).forget(result);
    NS_OK
}

/// Allocate a new MozURL object which is a clone of the original, and store a
/// pointer to it into newurl.
#[no_mangle]
pub extern "C" fn mozurl_clone(url: &MozURL, newurl: &mut *const MozURL) {
    MozURL::from_url(url.url.clone()).forget(newurl);
}

#[no_mangle]
pub extern "C" fn mozurl_spec(url: &MozURL) -> SpecSlice {
    url.as_ref().into()
}

#[no_mangle]
pub extern "C" fn mozurl_scheme(url: &MozURL) -> SpecSlice {
    url.scheme().into()
}

#[no_mangle]
pub extern "C" fn mozurl_username(url: &MozURL) -> SpecSlice {
    if url.cannot_be_a_base() {
        "".into()
    } else {
        url.username().into()
    }
}

#[no_mangle]
pub extern "C" fn mozurl_password(url: &MozURL) -> SpecSlice {
    url.password().unwrap_or("").into()
}

#[no_mangle]
pub extern "C" fn mozurl_host(url: &MozURL) -> SpecSlice {
    url.host_str().unwrap_or("").into()
}

#[no_mangle]
pub extern "C" fn mozurl_port(url: &MozURL) -> i32 {
    // NOTE: Gecko uses -1 to represent the default port.
    url.port().map(|p| p as i32).unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn mozurl_real_port(url: &MozURL) -> i32 {
    url.port()
        .or_else(|| default_port(url.scheme()))
        .map(|p| p as i32)
        .unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn mozurl_host_port(url: &MozURL) -> SpecSlice {
    (&url[Position::BeforeHost..Position::BeforePath]).into()
}

#[no_mangle]
pub extern "C" fn mozurl_filepath(url: &MozURL) -> SpecSlice {
    url.path().into()
}

#[no_mangle]
pub extern "C" fn mozurl_path(url: &MozURL) -> SpecSlice {
    (&url[Position::BeforePath..]).into()
}

#[no_mangle]
pub extern "C" fn mozurl_query(url: &MozURL) -> SpecSlice {
    url.query().unwrap_or("").into()
}

#[no_mangle]
pub extern "C" fn mozurl_fragment(url: &MozURL) -> SpecSlice {
    url.fragment().unwrap_or("").into()
}

#[no_mangle]
pub extern "C" fn mozurl_spec_no_ref(url: &MozURL) -> SpecSlice {
    (&url[..Position::AfterQuery]).into()
}

#[no_mangle]
pub extern "C" fn mozurl_has_fragment(url: &MozURL) -> bool {
    url.fragment().is_some()
}

#[no_mangle]
pub extern "C" fn mozurl_directory(url: &MozURL) -> SpecSlice {
    if let Some(position) = url.path().rfind('/') {
        url.path()[..position + 1].into()
    } else {
        url.path().into()
    }
}

#[no_mangle]
pub extern "C" fn mozurl_prepath(url: &MozURL) -> SpecSlice {
    (&url[..Position::BeforePath]).into()
}

fn get_origin(url: &MozURL) -> Option<String> {
    match url.scheme() {
        "blob" | "ftp" | "http" | "https" | "ws" | "wss" => {
            Some(url.origin().ascii_serialization())
        }
        "indexeddb" | "moz-extension" | "resource" => {
            let host = url.host_str().unwrap_or("");

            let port = url.port().or_else(|| default_port(url.scheme()));

            if port == default_port(url.scheme()) {
                Some(format!("{}://{}", url.scheme(), host))
            } else {
                Some(format!("{}://{}:{}", url.scheme(), host, port.unwrap()))
            }
        }
        "file" => {
            if unsafe { Gecko_StrictFileOriginPolicy() } {
                Some(url[..Position::AfterPath].to_owned())
            } else {
                Some("file://UNIVERSAL_FILE_URI_ORIGIN".to_owned())
            }
        }
        "about" | "moz-safe-about" => Some(url[..Position::AfterPath].to_owned()),
        _ => None,
    }
}

#[no_mangle]
pub extern "C" fn mozurl_origin(url: &MozURL, origin: &mut nsACString) {
    let origin_str = if !url.as_ref().starts_with("about:blank") {
        get_origin(url)
    } else {
        None
    };

    let origin_str = origin_str.unwrap_or_else(|| {
        // nsIPrincipal stores the uuid, so the same uuid is returned everytime.
        // We can't do that for MozURL because it can be used across threads.
        // Storing uuid would mutate the object which would cause races between
        // threads.
        format!("moz-nullprincipal:{{{}}}", Uuid::new_v4())
    });

    // NOTE: Try to re-use the allocation we got from rust-url, and transfer
    // ownership of the buffer to C++.
    let mut o = nsCString::from(origin_str);
    origin.take_from(&mut o);
}

fn get_base_domain(url: &MozURL) -> Result<Option<String>, nsresult> {
    match url.scheme() {
        "ftp" | "http" | "https" | "moz-extension" | "resource" => {
            let third_party_util: RefPtr<mozIThirdPartyUtil> =
                xpcom::components::ThirdPartyUtil::service()
                    .map_err(|_| NS_ERROR_ILLEGAL_DURING_SHUTDOWN)?;

            let scheme = nsCString::from(url.scheme());

            let mut host_str = url.host_str().unwrap_or("");

            if host_str.starts_with('[') && host_str.ends_with(']') {
                host_str = &host_str[1..host_str.len() - 1];
            }

            let host = nsCString::from(host_str);

            unsafe {
                let mut string = nsCString::new();
                third_party_util
                    .GetBaseDomainFromSchemeHost(&*scheme, &*host, &mut *string)
                    .to_result()?;

                // We know that GetBaseDomainFromSchemeHost returns AUTF8String, so just
                // use unwrap().
                Ok(Some(String::from_utf8(string.to_vec()).unwrap()))
            }
        }
        "ws" | "wss" => Ok(Some(url.as_ref().to_owned())),
        "file" => {
            if unsafe { Gecko_StrictFileOriginPolicy() } {
                Ok(Some(url.path().to_owned()))
            } else {
                Ok(Some("UNIVERSAL_FILE_URI_ORIGIN".to_owned()))
            }
        }
        "about" | "moz-safe-about" | "indexeddb" => Ok(Some(url.as_ref().to_owned())),
        _ => Ok(None),
    }
}

#[no_mangle]
pub extern "C" fn mozurl_base_domain(url: &MozURL, base_domain: &mut nsACString) -> nsresult {
    let base_domain_str = if !url.as_ref().starts_with("about:blank") {
        match get_base_domain(url) {
            Ok(domain) => domain,
            Err(rv) => return rv,
        }
    } else {
        None
    };

    let base_domain_str = base_domain_str.unwrap_or_else(|| {
        // See the comment in mozurl_origin about why we return a new uuid for
        // "moz-nullprincipals".
        format!("{{{}}}", Uuid::new_v4())
    });

    let mut bd = nsCString::from(base_domain_str);
    base_domain.take_from(&mut bd);

    NS_OK
}

// Helper macro for debug asserting that we're the only reference to MozURL.
macro_rules! debug_assert_mut {
    ($e:expr) => {
        debug_assert_eq!($e.refcnt.get(), 1, "Cannot mutate an aliased MozURL!");
    };
}

#[no_mangle]
pub extern "C" fn mozurl_set_scheme(url: &mut MozURL, scheme: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let scheme = try_or_malformed!(str::from_utf8(scheme));
    try_or_malformed!(quirks::set_protocol(url, scheme));
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_username(url: &mut MozURL, username: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let username = try_or_malformed!(str::from_utf8(username));
    try_or_malformed!(quirks::set_username(url, username));
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_password(url: &mut MozURL, password: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let password = try_or_malformed!(str::from_utf8(password));
    try_or_malformed!(quirks::set_password(url, password));
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_host_port(url: &mut MozURL, hostport: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let hostport = try_or_malformed!(str::from_utf8(hostport));
    try_or_malformed!(quirks::set_host(url, hostport));
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_hostname(url: &mut MozURL, host: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let host = try_or_malformed!(str::from_utf8(host));
    try_or_malformed!(quirks::set_hostname(url, host));
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_port_no(url: &mut MozURL, new_port: i32) -> nsresult {
    debug_assert_mut!(url);
    if url.cannot_be_a_base() {
        return NS_ERROR_MALFORMED_URI;
    }

    let port = match new_port {
        new if new < 0 || u16::max_value() as i32 <= new => None,
        new if Some(new as u16) == default_port(url.scheme()) => None,
        new => Some(new as u16),
    };
    try_or_malformed!(url.set_port(port));
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_pathname(url: &mut MozURL, path: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let path = try_or_malformed!(str::from_utf8(path));
    quirks::set_pathname(url, path);
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_query(url: &mut MozURL, query: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let query = try_or_malformed!(str::from_utf8(query));
    quirks::set_search(url, query);
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_set_fragment(url: &mut MozURL, fragment: &nsACString) -> nsresult {
    debug_assert_mut!(url);
    let fragment = try_or_malformed!(str::from_utf8(fragment));
    quirks::set_hash(url, fragment);
    NS_OK
}

#[no_mangle]
pub extern "C" fn mozurl_sizeof(url: &MozURL) -> usize {
    debug_assert_mut!(url);
    mem::size_of::<MozURL>() + url.as_str().len()
}

#[no_mangle]
pub extern "C" fn mozurl_common_base(
    url1: &MozURL,
    url2: &MozURL,
    result: &mut *const MozURL,
) -> nsresult {
    *result = ptr::null();
    if url1.url == url2.url {
        RefPtr::new(url1).forget(result);
        return NS_OK;
    }

    if url1.scheme() != url2.scheme()
        || url1.host() != url2.host()
        || url1.username() != url2.username()
        || url1.password() != url2.password()
        || url1.port() != url2.port()
    {
        return NS_OK;
    }

    match (url1.path_segments(), url2.path_segments()) {
        (Some(path1), Some(path2)) => {
            // Take the shared prefix of path segments
            let mut url = url1.url.clone();
            if let Ok(mut segs) = url.path_segments_mut() {
                segs.clear();
                segs.extend(path1.zip(path2).take_while(|&(a, b)| a == b).map(|p| p.0));
            } else {
                return NS_OK;
            }

            MozURL::from_url(url).forget(result);
            NS_OK
        }
        _ => NS_OK,
    }
}

#[no_mangle]
pub extern "C" fn mozurl_relative(
    url1: &MozURL,
    url2: &MozURL,
    result: &mut nsACString,
) -> nsresult {
    if url1.url == url2.url {
        result.truncate();
        return NS_OK;
    }

    if url1.scheme() != url2.scheme()
        || url1.host() != url2.host()
        || url1.username() != url2.username()
        || url1.password() != url2.password()
        || url1.port() != url2.port()
    {
        result.assign(url2.as_ref());
        return NS_OK;
    }

    match (url1.path_segments(), url2.path_segments()) {
        (Some(mut path1), Some(mut path2)) => {
            // Exhaust the part of the iterators that match
            while let (Some(ref p1), Some(ref p2)) = (path1.next(), path2.next()) {
                if p1 != p2 {
                    break;
                }
            }

            result.truncate();
            for _ in path1 {
                result.append("../");
            }
            for p2 in path2 {
                result.append(p2);
                result.append("/");
            }
        }
        _ => {
            result.assign(url2.as_ref());
        }
    }
    NS_OK
}

/// This type is used by nsStandardURL
#[no_mangle]
pub extern "C" fn rusturl_parse_ipv6addr(input: &nsACString, addr: &mut nsACString) -> nsresult {
    let ip6 = try_or_malformed!(str::from_utf8(input));
    let host = try_or_malformed!(url::Host::parse(ip6));
    let _ = write!(addr, "{}", host);
    NS_OK
}
