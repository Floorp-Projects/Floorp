/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate url;
use url::{Url, ParseOptions, Position};
use url::quirks;

extern crate libc;
use libc::size_t;

extern crate nsstring;
use nsstring::nsACString;

extern crate nserror;
use nserror::*;

use std::mem;
use std::str;
use std::ptr;

fn parser<'a>() -> ParseOptions<'a> {
   Url::options()
}

fn default_port(scheme: &str) -> Option<u32> {
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

#[no_mangle]
pub extern "C" fn rusturl_new(spec: &nsACString) -> *mut Url {
  let url_spec = match str::from_utf8(spec) {
    Ok(spec) => spec,
    Err(_) => return ptr::null_mut(),
  };

  match parser().parse(url_spec) {
    Ok(url) => Box::into_raw(Box::new(url)),
    Err(_) => return ptr::null_mut(),
  }
}

#[no_mangle]
pub extern "C" fn rusturl_clone(urlptr: Option<&Url>) -> *mut Url {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return ptr::null_mut();
  };

  return Box::into_raw(Box::new(url.clone()));
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_free(urlptr: *mut Url) {
  if urlptr.is_null() {
    return;
  }
  Box::from_raw(urlptr);
}

#[no_mangle]
pub extern "C" fn rusturl_get_spec(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign(url.as_ref());
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_scheme(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign(&url.scheme());
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_username(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  if url.cannot_be_a_base() {
      cont.assign("");
  } else {
      cont.assign(url.username());
  }
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_password(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign(url.password().unwrap_or(""));
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_host(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign(url.host_str().unwrap_or(""));
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_port(urlptr: Option<&Url>, port: &mut i32) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  match url.port() {
    Some(p) => {
      *port = p as i32;
    }
    None => {
      // NOTE: Gecko uses -1 to represent the default port
      *port = -1;
    }
  }
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_filepath(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  if url.cannot_be_a_base() {
      cont.assign("");
  } else {
      cont.assign(&url[Position::BeforePath..Position::AfterPath]);
  }
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_path(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  if url.cannot_be_a_base() {
      cont.assign("");
  } else {
      cont.assign(&url[Position::BeforePath..]);
  }
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_query(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign(url.query().unwrap_or(""));
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_get_fragment(urlptr: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign(url.fragment().unwrap_or(""));
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_has_fragment(urlptr: Option<&Url>, has_fragment: &mut bool) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  *has_fragment = url.fragment().is_some();
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_set_scheme(urlptr: Option<&mut Url>, scheme: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let scheme_ = match str::from_utf8(scheme) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  match quirks::set_protocol(url, scheme_) {
    Ok(()) => NS_OK,
    Err(()) => NS_ERROR_MALFORMED_URI,
  }
}


#[no_mangle]
pub extern "C" fn rusturl_set_username(urlptr: Option<&mut Url>, username: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let username_ = match str::from_utf8(username) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  match quirks::set_username(url, username_) {
    Ok(()) => NS_OK,
    Err(()) => NS_ERROR_MALFORMED_URI,
  }
}

#[no_mangle]
pub extern "C" fn rusturl_set_password(urlptr: Option<&mut Url>, password: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let password_ = match str::from_utf8(password) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  match quirks::set_password(url, password_) {
    Ok(()) => NS_OK,
    Err(()) => NS_ERROR_MALFORMED_URI,
  }
}

#[no_mangle]
pub extern "C" fn rusturl_set_host_port(urlptr: Option<&mut Url>, host_port: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let host_port_ = match str::from_utf8(host_port) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  match quirks::set_host(url, host_port_) {
    Ok(()) => NS_OK,
    Err(()) => NS_ERROR_MALFORMED_URI,
  }
}

#[no_mangle]
pub extern "C" fn rusturl_set_host_and_port(urlptr: Option<&mut Url>, host_and_port: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let _ = url.set_port(None);
  let host_and_port_ = match str::from_utf8(host_and_port) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  match quirks::set_host(url, host_and_port_) {
    Ok(()) => NS_OK,
    Err(()) => NS_ERROR_MALFORMED_URI,
  }
}

#[no_mangle]
pub extern "C" fn rusturl_set_host(urlptr: Option<&mut Url>, host: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let hostname = match str::from_utf8(host) {
    Ok(h) => h,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  match quirks::set_hostname(url, hostname) {
    Ok(()) => NS_OK,
    Err(()) => NS_ERROR_MALFORMED_URI,
  }
}

#[no_mangle]
pub extern "C" fn rusturl_set_port(urlptr: Option<&mut Url>, port: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let port_ = match str::from_utf8(port) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  match quirks::set_port(url, port_) {
    Ok(()) => NS_OK,
    Err(()) => NS_ERROR_MALFORMED_URI,
  }
}

#[no_mangle]
pub extern "C" fn rusturl_set_port_no(urlptr: Option<&mut Url>, new_port: i32) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  if url.cannot_be_a_base() {
    return NS_ERROR_MALFORMED_URI;
  }

  if url.scheme() == "file" {
    return NS_ERROR_MALFORMED_URI;
  }
  match default_port(url.scheme()) {
    Some(def_port) => if new_port == def_port as i32 {
      let _ = url.set_port(None);
      return NS_OK;
    },
    None => {}
  };

  if new_port > std::u16::MAX as i32 || new_port < 0 {
      let _ = url.set_port(None);
  } else {
      let _ = url.set_port(Some(new_port as u16));
  }

  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_set_path(urlptr: Option<&mut Url>, path: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let path_ = match str::from_utf8(path) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  quirks::set_pathname(url, path_);
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_set_query(urlptr: Option<&mut Url>, query: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let query_ = match str::from_utf8(query) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  quirks::set_search(url, query_);
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_set_fragment(urlptr: Option<&mut Url>, fragment: &nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let fragment_ = match str::from_utf8(fragment) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_MALFORMED_URI, // utf-8 failed
  };

  quirks::set_hash(url, fragment_);
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_resolve(urlptr: Option<&Url>, resolve: &nsACString, cont: &mut nsACString) -> nsresult {
  let url = if let Some(url) = urlptr {
    url
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  let resolve_ = match str::from_utf8(resolve) {
    Ok(p) => p,
    Err(_) => return NS_ERROR_FAILURE,
  };

  if let Ok(ref u) = parser().base_url(Some(&url)).parse(resolve_) {
    cont.assign(u.as_ref());
  } else {
    cont.assign("");
  }
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_common_base_spec(urlptr1: Option<&Url>, urlptr2: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let (url1, url2) = if let (Some(url1), Some(url2)) = (urlptr1, urlptr2) {
    (url1, url2)
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign("");

  if url1 == url2 {
    cont.assign(url1.as_ref());
    return NS_OK;
  }

  if url1.scheme() != url2.scheme() ||
     url1.host() != url2.host() ||
     url1.username() != url2.username() ||
     url1.password() != url2.password() ||
     url1.port() != url2.port() {
    return NS_OK;
  }

  let path1 = match url1.path_segments() {
    Some(path) => path,
    None => return NS_OK,
  };
  let path2 = match url2.path_segments() {
    Some(path) => path,
    None => return NS_OK,
  };

  let mut url = url1.clone();
  url.set_query(None);
  let _ = url.set_host(None);
  {
    let mut new_segments = if let Ok(segments) = url.path_segments_mut() {
      segments
    } else {
      return NS_OK;
    };

    for (p1, p2) in path1.zip(path2) {
      if p1 != p2 {
        break;
      } else {
        new_segments.push(p1);
      }
    }
  }

  cont.assign(url.as_ref());
  NS_OK
}

#[no_mangle]
pub extern "C" fn rusturl_relative_spec(urlptr1: Option<&Url>, urlptr2: Option<&Url>, cont: &mut nsACString) -> nsresult {
  let (url1, url2) = if let (Some(url1), Some(url2)) = (urlptr1, urlptr2) {
    (url1, url2)
  } else {
    return NS_ERROR_INVALID_ARG;
  };

  cont.assign("");

  if url1 == url2 {
    return NS_OK;
  }

  if url1.scheme() != url2.scheme() ||
     url1.host() != url2.host() ||
     url1.username() != url2.username() ||
     url1.password() != url2.password() ||
     url1.port() != url2.port() {
    cont.assign(url2.as_ref());
    return NS_OK;
  }

  let mut path1 = match url1.path_segments() {
    Some(path) => path,
    None => {
      cont.assign(url2.as_ref());
      return NS_OK;
    }
  };
  let mut path2 = match url2.path_segments() {
    Some(path) => path,
    None => {
      cont.assign(url2.as_ref());
      return NS_OK;
    }
  };

  // TODO: file:// on WIN?

  // Exhaust the part of the iterators that match
  while let (Some(ref p1), Some(ref p2)) = (path1.next(), path2.next()) {
    if p1 != p2 {
      break;
    }
  }

  let mut buffer = String::new();
  for _ in path1 {
    buffer.push_str("../");
  }
  for p2 in path2 {
    buffer.push_str(p2);
    buffer.push('/');
  }

  cont.assign(&buffer);
  NS_OK
}

#[no_mangle]
pub extern "C" fn sizeof_rusturl() -> size_t {
  mem::size_of::<Url>()
}

#[no_mangle]
pub extern "C" fn rusturl_parse_ipv6addr(input: &nsACString, cont: &mut nsACString) -> nsresult {
  let ip6 = match str::from_utf8(input) {
    Ok(content) => content,
    Err(_) => return NS_ERROR_FAILURE,
  };

  let h = match url::Host::parse(ip6) {
    Ok(host) => host,
    // XXX: Do we want to change our error message based on the error type?
    Err(_) => return NS_ERROR_MALFORMED_URI,
  };

  cont.assign(&h.to_string());
  NS_OK
}
