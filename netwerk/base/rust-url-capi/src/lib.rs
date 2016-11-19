/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate url;
use url::{Url, ParseError, ParseOptions, Position};
use url::quirks;

extern crate libc;
use libc::size_t;

extern crate nsstring;
use nsstring::nsACString;


use std::mem;
use std::str;
use std::ptr;

#[allow(non_camel_case_types)]
pub type rusturl_ptr = *const libc::c_void;

mod error_mapping;
use error_mapping::*;

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
pub unsafe extern "C" fn rusturl_new(spec: &nsACString) -> rusturl_ptr {
  let url_spec = match str::from_utf8(spec) {
    Ok(spec) => spec,
    Err(_) => return ptr::null(),
  };

  match parser().parse(url_spec) {
    Ok(url) => Box::into_raw(Box::new(url)) as rusturl_ptr,
    Err(_) => return ptr::null(),
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_free(urlptr: rusturl_ptr) {
  if urlptr.is_null() {
    return ();
  }
  let url: Box<Url> = Box::from_raw(urlptr as *mut url::Url);
  drop(url);
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_spec(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url = &*(urlptr as *const Url);
  cont.assign(url.as_ref());
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_scheme(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  cont.assign(&url.scheme());
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_username(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  if url.cannot_be_a_base() {
      cont.assign("");
  } else {
      cont.assign(url.username());
  }
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_password(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  cont.assign(url.password().unwrap_or(""));
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_host(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  cont.assign(url.host_str().unwrap_or(""));
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_port(urlptr: rusturl_ptr) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  match url.port() {
    Some(port) => port as i32,
    None => -1
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_path(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  if url.cannot_be_a_base() {
      cont.assign("");
  } else {
      cont.assign(&url[Position::BeforePath..]);
  }
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_query(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  cont.assign(url.query().unwrap_or(""));
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_fragment(urlptr: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  cont.assign(url.fragment().unwrap_or(""));
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_has_fragment(urlptr: rusturl_ptr) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  url.fragment().is_some() as i32
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_scheme(urlptr: rusturl_ptr, scheme: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let scheme_ = match str::from_utf8(scheme) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_protocol(url, scheme_).error_code()
}


#[no_mangle]
pub unsafe extern "C" fn rusturl_set_username(urlptr: rusturl_ptr, username: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let username_ = match str::from_utf8(username) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_username(url, username_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_password(urlptr: rusturl_ptr, password: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let password_ = match str::from_utf8(password) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_password(url, password_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_host_port(urlptr: rusturl_ptr, host_port: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let host_port_ = match str::from_utf8(host_port) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_host(url, host_port_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_host_and_port(urlptr: rusturl_ptr, host_and_port: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let _ = url.set_port(None);

  let host_and_port_ = match str::from_utf8(host_and_port) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_host(url, host_and_port_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_host(urlptr: rusturl_ptr, host: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let hostname = match str::from_utf8(host) {
    Ok(h) => h,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_hostname(url, hostname).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_port(urlptr: rusturl_ptr, port: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let port_ = match str::from_utf8(port) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_port(url, port_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_port_no(urlptr: rusturl_ptr, new_port: i32) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  if url.cannot_be_a_base() {
      -100
  } else {
    if url.scheme() == "file" {
        return -100;
    }
    match default_port(url.scheme()) {
      Some(def_port) => if new_port == def_port as i32 {
        let _ = url.set_port(None);
        return NSError::OK.error_code();
      },
      None => {}
    };
    if new_port > std::u16::MAX as i32 || new_port < 0 {
      let _ = url.set_port(None);
    } else {
      let _ = url.set_port(Some(new_port as u16));
    }
    NSError::OK.error_code()
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_path(urlptr: rusturl_ptr, path: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let path_ = match str::from_utf8(path) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_pathname(url, path_);
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_query(urlptr: rusturl_ptr, query: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let query_ = match str::from_utf8(query) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_search(url, query_);
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_fragment(urlptr: rusturl_ptr, fragment: &nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);

  let fragment_ = match str::from_utf8(fragment) {
    Ok(p) => p,
    Err(_) => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_hash(url, fragment_);
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_resolve(urlptr: rusturl_ptr, resolve: &nsACString, cont: &mut nsACString) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  let resolve_ = match str::from_utf8(resolve) {
    Ok(p) => p,
    Err(_) => return NSError::Failure.error_code()
  };

  if let Ok(ref u) = parser().base_url(Some(&url)).parse(resolve_) {
    cont.assign(u.as_ref());
  } else {
    cont.assign("");
  }
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_common_base_spec(urlptr1: rusturl_ptr, urlptr2: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr1.is_null() || urlptr2.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url1: &Url = mem::transmute(urlptr1);
  let url2: &Url = mem::transmute(urlptr2);

  cont.assign("");

  if url1 == url2 {
    cont.assign(url1.as_ref());
    return NSError::OK.error_code();
  }

  if url1.scheme() != url2.scheme() ||
     url1.host() != url2.host() ||
     url1.username() != url2.username() ||
     url1.password() != url2.password() ||
     url1.port() != url2.port() {
    return NSError::OK.error_code();
  }

  let path1 = match url1.path_segments() {
    Some(path) => path,
    None => return NSError::OK.error_code(),
  };
  let path2 = match url2.path_segments() {
    Some(path) => path,
    None => return NSError::OK.error_code(),
  };

  let mut url = url1.clone();
  url.set_query(None);
  let _ = url.set_host(None);
  {
    let mut new_segments = if let Ok(segments) = url.path_segments_mut() {
      segments
    } else {
      return NSError::OK.error_code();
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
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_relative_spec(urlptr1: rusturl_ptr, urlptr2: rusturl_ptr, cont: &mut nsACString) -> i32 {
  if urlptr1.is_null() || urlptr2.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url1: &Url = mem::transmute(urlptr1);
  let url2: &Url = mem::transmute(urlptr2);

  cont.assign("");

  if url1 == url2 {
    return NSError::OK.error_code();
  }

  if url1.scheme() != url2.scheme() ||
     url1.host() != url2.host() ||
     url1.username() != url2.username() ||
     url1.password() != url2.password() ||
     url1.port() != url2.port() {
    cont.assign(url2.as_ref());
    return NSError::OK.error_code();
  }

  let mut path1 = match url1.path_segments() {
    Some(path) => path,
    None => {
      cont.assign(url2.as_ref());
      return NSError::OK.error_code()
    }
  };
  let mut path2 = match url2.path_segments() {
    Some(path) => path,
    None => {
      cont.assign(url2.as_ref());
      return NSError::OK.error_code()
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
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn sizeof_rusturl() -> size_t {
  mem::size_of::<Url>()
}
