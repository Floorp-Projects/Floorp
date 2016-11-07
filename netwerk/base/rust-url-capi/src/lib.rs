/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate url;
use url::{Url, ParseError, ParseOptions};
use url::quirks;
extern crate libc;
use libc::size_t;


use std::mem;
use std::str;

#[allow(non_camel_case_types)]
pub type rusturl_ptr = *const libc::c_void;

mod string_utils;
pub use string_utils::*;

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
pub unsafe extern "C" fn rusturl_new(spec: *mut libc::c_char, len: size_t) -> rusturl_ptr {
  let slice = std::slice::from_raw_parts(spec as *const libc::c_uchar, len as usize);
  let url_spec = match str::from_utf8(slice) {
    Ok(spec) => spec,
    Err(_) => return 0 as rusturl_ptr
  };

  let url = match parser().parse(url_spec) {
    Ok(url) => url,
    Err(_) => return 0 as rusturl_ptr
  };

  let url = Box::new(url);
  Box::into_raw(url) as rusturl_ptr
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
pub unsafe extern "C" fn rusturl_get_spec(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  cont.assign(&url.to_string())
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_scheme(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  cont.assign(&url.scheme())
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_username(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  if url.cannot_be_a_base() {
      cont.set_size(0)
  } else {
      cont.assign(url.username())
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_password(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  match url.password() {
    Some(p) => cont.assign(&p.to_string()),
    None => cont.set_size(0)
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_host(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  match url.host() {
    Some(h) => cont.assign(&h.to_string()),
    None => cont.set_size(0)
  }
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
pub unsafe extern "C" fn rusturl_get_path(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  if url.cannot_be_a_base() {
      cont.set_size(0)
  } else {
      cont.assign(url.path())
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_query(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);
  match url.query() {
    Some(ref s) => cont.assign(s),
    None => cont.set_size(0)
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_get_fragment(urlptr: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  match url.fragment() {
    Some(ref fragment) => cont.assign(fragment),
    None => cont.set_size(0)
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_has_fragment(urlptr: rusturl_ptr) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &Url = mem::transmute(urlptr);

  match url.fragment() {
    Some(_) => return 1,
    None => return 0
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_scheme(urlptr: rusturl_ptr, scheme: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(scheme as *const libc::c_uchar, len as usize);

  let scheme_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_protocol(url, scheme_).error_code()
}


#[no_mangle]
pub unsafe extern "C" fn rusturl_set_username(urlptr: rusturl_ptr, username: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(username as *const libc::c_uchar, len as usize);

  let username_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_username(url, username_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_password(urlptr: rusturl_ptr, password: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(password as *const libc::c_uchar, len as usize);

  let password_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_password(url, password_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_host_port(urlptr: rusturl_ptr, host_port: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(host_port as *const libc::c_uchar, len as usize);

  let host_port_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_host(url, host_port_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_host_and_port(urlptr: rusturl_ptr, host_and_port: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  url.set_port(None);
  let slice = std::slice::from_raw_parts(host_and_port as *const libc::c_uchar, len as usize);

  let host_and_port_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_host(url, host_and_port_).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_host(urlptr: rusturl_ptr, host: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(host as *const libc::c_uchar, len as usize);

  let hostname = match str::from_utf8(slice).ok() {
    Some(h) => h,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_hostname(url, hostname).error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_port(urlptr: rusturl_ptr, port: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(port as *const libc::c_uchar, len as usize);

  let port_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
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
pub unsafe extern "C" fn rusturl_set_path(urlptr: rusturl_ptr, path: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(path as *const libc::c_uchar, len as usize);

  let path_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_pathname(url, path_);
  NSError::OK.error_code()

}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_query(urlptr: rusturl_ptr, query: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(query as *const libc::c_uchar, len as usize);

  let query_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_search(url, query_);
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_set_fragment(urlptr: rusturl_ptr, fragment: *mut libc::c_char, len: size_t) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let mut url: &mut Url = mem::transmute(urlptr);
  let slice = std::slice::from_raw_parts(fragment as *const libc::c_uchar, len as usize);

  let fragment_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return ParseError::InvalidDomainCharacter.error_code() // utf-8 failed
  };

  quirks::set_hash(url, fragment_);
  NSError::OK.error_code()
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_resolve(urlptr: rusturl_ptr, resolve: *mut libc::c_char, len: size_t, cont: *mut libc::c_void) -> i32 {
  if urlptr.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url: &mut Url = mem::transmute(urlptr);

    let slice = std::slice::from_raw_parts(resolve as *const libc::c_uchar, len as usize);

  let resolve_ = match str::from_utf8(slice).ok() {
    Some(p) => p,
    None => return NSError::Failure.error_code()
  };

  match parser().base_url(Some(&url)).parse(resolve_).ok() {
    Some(u) => cont.assign(&u.to_string()),
    None => cont.set_size(0)
  }
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_common_base_spec(urlptr1: rusturl_ptr, urlptr2: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr1.is_null() || urlptr2.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url1: &Url = mem::transmute(urlptr1);
  let url2: &Url = mem::transmute(urlptr2);

  if url1 == url2 {
    return cont.assign(&url1.to_string());
  }

  if url1.scheme() != url2.scheme() ||
     url1.host() != url2.host() ||
     url1.username() != url2.username() ||
     url1.password() != url2.password() ||
     url1.port() != url2.port() {
    return cont.set_size(0);
  }

  let path1 = match url1.path_segments() {
    Some(path) => path,
    None => return cont.set_size(0)
  };
  let path2 = match url2.path_segments() {
    Some(path) => path,
    None => return cont.set_size(0)
  };

  let mut url = url1.clone();
  url.set_query(None);
  let _ = url.set_host(None);
  {
    let mut new_segments = if let Ok(segments) = url.path_segments_mut() {
      segments
    } else {
      return cont.set_size(0)
    };

    for (p1, p2) in path1.zip(path2) {
      if p1 != p2 {
        break;
      } else {
          new_segments.push(p1);
      }
    }
  }

  cont.assign(&url.to_string())
}

#[no_mangle]
pub unsafe extern "C" fn rusturl_relative_spec(urlptr1: rusturl_ptr, urlptr2: rusturl_ptr, cont: *mut libc::c_void) -> i32 {
  if urlptr1.is_null() || urlptr2.is_null() {
    return NSError::InvalidArg.error_code();
  }
  let url1: &Url = mem::transmute(urlptr1);
  let url2: &Url = mem::transmute(urlptr2);

  if url1 == url2 {
    return cont.set_size(0);
  }

  if url1.scheme() != url2.scheme() ||
     url1.host() != url2.host() ||
     url1.username() != url2.username() ||
     url1.password() != url2.password() ||
     url1.port() != url2.port() {
    return cont.assign(&url2.to_string());
  }

  let mut path1 = match url1.path_segments() {
    Some(path) => path,
    None => return cont.assign(&url2.to_string())
  };
  let mut path2 = match url2.path_segments() {
    Some(path) => path,
    None => return cont.assign(&url2.to_string())
  };

  // TODO: file:// on WIN?

  // Exhaust the part of the iterators that match
  while let (Some(ref p1), Some(ref p2)) = (path1.next(), path2.next()) {
    if p1 != p2 {
      break;
    }
  }

  let mut buffer: String = "".to_string();
  for _ in path1 {
    buffer = buffer + "../";
  }
  for p2 in path2 {
    buffer = buffer + p2 + "/";
  }

  return cont.assign(&buffer);
}

#[no_mangle]
pub unsafe extern "C" fn sizeof_rusturl() -> size_t {
  mem::size_of::<Url>()
}
