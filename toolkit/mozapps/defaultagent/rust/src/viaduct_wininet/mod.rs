// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

use winapi::shared::winerror::ERROR_INSUFFICIENT_BUFFER;
use winapi::um::errhandlingapi::GetLastError;
use winapi::um::wininet;
use wio::wide::ToWide;

use viaduct::Backend;

mod internet_handle;
use internet_handle::InternetHandle;

pub struct WinInetBackend;

/// Errors
fn to_viaduct_error(e: u32) -> viaduct::Error {
    // Like "0xabcde".
    viaduct::Error::BackendError(format!("{:#x}", e))
}

fn get_status(req: wininet::HINTERNET) -> Result<u16, viaduct::Error> {
    let mut status: u32 = 0;
    let mut size: u32 = std::mem::size_of::<u32>() as u32;
    let result = unsafe {
        wininet::HttpQueryInfoW(
            req,
            wininet::HTTP_QUERY_STATUS_CODE | wininet::HTTP_QUERY_FLAG_NUMBER,
            &mut status as *mut _ as *mut _,
            &mut size,
            std::ptr::null_mut(),
        )
    };
    if 0 == result {
        return Err(to_viaduct_error(unsafe { GetLastError() }));
    }

    Ok(status as u16)
}

fn get_headers(req: wininet::HINTERNET) -> Result<viaduct::Headers, viaduct::Error> {
    // We follow https://docs.microsoft.com/en-us/windows/win32/wininet/retrieving-http-headers.
    //
    // Per
    // https://docs.microsoft.com/en-us/windows/win32/api/wininet/nf-wininet-httpqueryinfoa:
    // The `HttpQueryInfoA` function represents headers as ISO-8859-1 characters
    // not ANSI characters.
    let mut size: u32 = 0;

    let result = unsafe {
        wininet::HttpQueryInfoA(
            req,
            wininet::HTTP_QUERY_RAW_HEADERS,
            std::ptr::null_mut(),
            &mut size,
            std::ptr::null_mut(),
        )
    };
    if 0 == result {
        let error = unsafe { GetLastError() };
        if error == wininet::ERROR_HTTP_HEADER_NOT_FOUND {
            return Ok(viaduct::Headers::new());
        } else if error != ERROR_INSUFFICIENT_BUFFER {
            return Err(to_viaduct_error(error));
        }
    }

    let mut buffer = vec![0 as u8; size as usize];
    let result = unsafe {
        wininet::HttpQueryInfoA(
            req,
            wininet::HTTP_QUERY_RAW_HEADERS,
            buffer.as_mut_ptr() as *mut _,
            &mut size,
            std::ptr::null_mut(),
        )
    };
    if 0 == result {
        let error = unsafe { GetLastError() };
        if error == wininet::ERROR_HTTP_HEADER_NOT_FOUND {
            return Ok(viaduct::Headers::new());
        } else {
            return Err(to_viaduct_error(error));
        }
    }

    // The API returns all of the headers as a single char buffer in
    // ISO-8859-1 encoding.  Each header is terminated by '\0' and
    // there's a trailing '\0' terminator as well.
    //
    // We want UTF-8.  It's not worth include a non-trivial encoding
    // library like `encoding_rs` just for these headers, so let's use
    // the fact that ISO-8859-1 and UTF-8 intersect on the lower 7 bits
    // and decode lossily.  It will at least be reasonably clear when
    // there is an encoding issue.
    let allheaders = String::from_utf8_lossy(&buffer);

    let mut headers = viaduct::Headers::new();
    for header in allheaders.split(0 as char) {
        let mut it = header.splitn(2, ":");
        if let (Some(name), Some(value)) = (it.next(), it.next()) {
            headers.insert(name.trim().to_string(), value.trim().to_string())?;
        }
    }

    return Ok(headers);
}

fn get_body(req: wininet::HINTERNET) -> Result<Vec<u8>, viaduct::Error> {
    let mut body = Vec::new();

    const BUFFER_SIZE: usize = 65535;
    let mut buffer: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];

    loop {
        let mut bytes_downloaded: u32 = 0;
        let result = unsafe {
            wininet::InternetReadFile(
                req,
                buffer.as_mut_ptr() as *mut _,
                BUFFER_SIZE as u32,
                &mut bytes_downloaded,
            )
        };
        if 0 == result {
            return Err(to_viaduct_error(unsafe { GetLastError() }));
        }
        if bytes_downloaded == 0 {
            break;
        }

        body.extend_from_slice(&buffer[0..bytes_downloaded as usize]);
    }
    Ok(body)
}

impl Backend for WinInetBackend {
    fn send(&self, request: viaduct::Request) -> Result<viaduct::Response, viaduct::Error> {
        viaduct::note_backend("wininet.dll");

        let request_method = request.method;
        let url = request.url;

        let session = unsafe {
            InternetHandle::new(wininet::InternetOpenW(
                "DefaultAgent/1.0".to_wide_null().as_ptr(),
                wininet::INTERNET_OPEN_TYPE_PRECONFIG,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                0,
            ))
        }
        .map_err(to_viaduct_error)?;

        // Consider asserting the scheme here too, for documentation purposes.
        // Viaduct itself only allows HTTPS at this time, but that might change.
        let host = url
            .host_str()
            .ok_or(viaduct::Error::BackendError("no host".to_string()))?;

        let conn = unsafe {
            InternetHandle::new(wininet::InternetConnectW(
                session.as_raw(),
                host.to_wide_null().as_ptr(),
                wininet::INTERNET_DEFAULT_HTTPS_PORT as u16,
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                wininet::INTERNET_SERVICE_HTTP,
                0,
                0,
            ))
        }
        .map_err(to_viaduct_error)?;

        let path = url[url::Position::BeforePath..].to_string();
        let req = unsafe {
            wininet::HttpOpenRequestW(
                conn.as_raw(),
                request_method.as_str().to_wide_null().as_ptr(),
                path.to_wide_null().as_ptr(),
                std::ptr::null_mut(), /* lpszVersion */
                std::ptr::null_mut(), /* lpszReferrer */
                std::ptr::null_mut(), /* lplpszAcceptTypes */
                // Avoid the cache as best we can.
                wininet::INTERNET_FLAG_NO_AUTH
                    | wininet::INTERNET_FLAG_NO_CACHE_WRITE
                    | wininet::INTERNET_FLAG_NO_COOKIES
                    | wininet::INTERNET_FLAG_NO_UI
                    | wininet::INTERNET_FLAG_PRAGMA_NOCACHE
                    | wininet::INTERNET_FLAG_RELOAD
                    | wininet::INTERNET_FLAG_SECURE,
                0,
            )
        };
        if req.is_null() {
            return Err(to_viaduct_error(unsafe { GetLastError() }));
        }

        for header in request.headers {
            // Per
            // https://docs.microsoft.com/en-us/windows/win32/api/wininet/nf-wininet-httpaddrequestheadersw,
            // "Each header must be terminated by a CR/LF (carriage return/line
            // feed) pair."
            let h = format!("{}: {}\r\n", header.name(), header.value());
            let result = unsafe {
                wininet::HttpAddRequestHeadersW(
                    req,
                    h.to_wide_null().as_ptr(), /* lpszHeaders */
                    -1i32 as u32,              /* dwHeadersLength */
                    wininet::HTTP_ADDREQ_FLAG_ADD | wininet::HTTP_ADDREQ_FLAG_REPLACE, /* dwModifiers */
                )
            };
            if 0 == result {
                return Err(to_viaduct_error(unsafe { GetLastError() }));
            }
        }

        // Future work: support sending a body.
        if request.body.is_some() {
            return Err(viaduct::Error::BackendError(
                "non-empty body is not yet supported".to_string(),
            ));
        }

        let result = unsafe {
            wininet::HttpSendRequestW(
                req,
                std::ptr::null_mut(), /* lpszHeaders */
                0,                    /* dwHeadersLength */
                std::ptr::null_mut(), /* lpOptional */
                0,                    /* dwOptionalLength */
            )
        };
        if 0 == result {
            return Err(to_viaduct_error(unsafe { GetLastError() }));
        }

        let status = get_status(req)?;
        let headers = get_headers(req)?;

        // Not all responses have a body.
        let has_body = headers.get_header("content-type").is_some()
            || headers.get_header("content-length").is_some();
        let body = if has_body { get_body(req)? } else { Vec::new() };

        Ok(viaduct::Response {
            request_method,
            body,
            url,
            status,
            headers,
        })
    }
}
