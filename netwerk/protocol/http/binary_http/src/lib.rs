/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate bhttp;
extern crate nserror;
extern crate nsstring;
extern crate thin_vec;
#[macro_use]
extern crate xpcom;

use bhttp::{Message, Mode};
use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG, NS_ERROR_UNEXPECTED, NS_OK};
use nsstring::{nsACString, nsCString};
use std::io::Cursor;
use thin_vec::ThinVec;
use xpcom::interfaces::{nsIBinaryHttpRequest, nsIBinaryHttpResponse};
use xpcom::RefPtr;

enum HeaderComponent {
    Name,
    Value,
}

// Extracts either the names or the values of a slice of header (name, value) pairs.
fn extract_header_components(
    headers: &[(Vec<u8>, Vec<u8>)],
    component: HeaderComponent,
) -> ThinVec<nsCString> {
    let mut header_components = ThinVec::with_capacity(headers.len());
    for (name, value) in headers {
        match component {
            HeaderComponent::Name => header_components.push(nsCString::from(name.clone())),
            HeaderComponent::Value => header_components.push(nsCString::from(value.clone())),
        }
    }
    header_components
}

#[xpcom(implement(nsIBinaryHttpRequest), atomic)]
struct BinaryHttpRequest {
    method: Vec<u8>,
    scheme: Vec<u8>,
    authority: Vec<u8>,
    path: Vec<u8>,
    headers: Vec<(Vec<u8>, Vec<u8>)>,
    content: Vec<u8>,
}

impl BinaryHttpRequest {
    xpcom_method!(get_method => GetMethod() -> nsACString);
    fn get_method(&self) -> Result<nsCString, nsresult> {
        Ok(nsCString::from(self.method.clone()))
    }

    xpcom_method!(get_scheme => GetScheme() -> nsACString);
    fn get_scheme(&self) -> Result<nsCString, nsresult> {
        Ok(nsCString::from(self.scheme.clone()))
    }

    xpcom_method!(get_authority => GetAuthority() -> nsACString);
    fn get_authority(&self) -> Result<nsCString, nsresult> {
        Ok(nsCString::from(self.authority.clone()))
    }

    xpcom_method!(get_path => GetPath() -> nsACString);
    fn get_path(&self) -> Result<nsCString, nsresult> {
        Ok(nsCString::from(self.path.clone()))
    }

    xpcom_method!(get_content => GetContent() -> ThinVec<u8>);
    fn get_content(&self) -> Result<ThinVec<u8>, nsresult> {
        Ok(self.content.clone().into_iter().collect())
    }

    xpcom_method!(get_header_names => GetHeaderNames() -> ThinVec<nsCString>);
    fn get_header_names(&self) -> Result<ThinVec<nsCString>, nsresult> {
        Ok(extract_header_components(
            &self.headers,
            HeaderComponent::Name,
        ))
    }

    xpcom_method!(get_header_values => GetHeaderValues() -> ThinVec<nsCString>);
    fn get_header_values(&self) -> Result<ThinVec<nsCString>, nsresult> {
        Ok(extract_header_components(
            &self.headers,
            HeaderComponent::Value,
        ))
    }
}

#[xpcom(implement(nsIBinaryHttpResponse), atomic)]
struct BinaryHttpResponse {
    status: u16,
    headers: Vec<(Vec<u8>, Vec<u8>)>,
    content: Vec<u8>,
}

impl BinaryHttpResponse {
    xpcom_method!(get_status => GetStatus() -> u16);
    fn get_status(&self) -> Result<u16, nsresult> {
        Ok(self.status)
    }

    xpcom_method!(get_content => GetContent() -> ThinVec<u8>);
    fn get_content(&self) -> Result<ThinVec<u8>, nsresult> {
        Ok(self.content.clone().into_iter().collect())
    }

    xpcom_method!(get_header_names => GetHeaderNames() -> ThinVec<nsCString>);
    fn get_header_names(&self) -> Result<ThinVec<nsCString>, nsresult> {
        Ok(extract_header_components(
            &self.headers,
            HeaderComponent::Name,
        ))
    }

    xpcom_method!(get_header_values => GetHeaderValues() -> ThinVec<nsCString>);
    fn get_header_values(&self) -> Result<ThinVec<nsCString>, nsresult> {
        Ok(extract_header_components(
            &self.headers,
            HeaderComponent::Value,
        ))
    }
}

#[xpcom(implement(nsIBinaryHttp), atomic)]
struct BinaryHttp {}

impl BinaryHttp {
    xpcom_method!(encode_request => EncodeRequest(request: *const nsIBinaryHttpRequest) -> ThinVec<u8>);
    fn encode_request(&self, request: &nsIBinaryHttpRequest) -> Result<ThinVec<u8>, nsresult> {
        let mut method = nsCString::new();
        unsafe { request.GetMethod(&mut *method) }.to_result()?;
        let mut scheme = nsCString::new();
        unsafe { request.GetScheme(&mut *scheme) }.to_result()?;
        let mut authority = nsCString::new();
        unsafe { request.GetAuthority(&mut *authority) }.to_result()?;
        let mut path = nsCString::new();
        unsafe { request.GetPath(&mut *path) }.to_result()?;
        let mut message = Message::request(
            method.to_vec(),
            scheme.to_vec(),
            authority.to_vec(),
            path.to_vec(),
        );
        let mut header_names = ThinVec::new();
        unsafe { request.GetHeaderNames(&mut header_names) }.to_result()?;
        let mut header_values = ThinVec::with_capacity(header_names.len());
        unsafe { request.GetHeaderValues(&mut header_values) }.to_result()?;
        if header_names.len() != header_values.len() {
            return Err(NS_ERROR_INVALID_ARG);
        }
        for (name, value) in header_names.iter().zip(header_values.iter()) {
            message.put_header(name.to_vec(), value.to_vec());
        }
        let mut content = ThinVec::new();
        unsafe { request.GetContent(&mut content) }.to_result()?;
        message.write_content(content);
        let mut encoded = ThinVec::new();
        message
            .write_bhttp(Mode::KnownLength, &mut encoded)
            .map_err(|_| NS_ERROR_FAILURE)?;
        Ok(encoded)
    }

    xpcom_method!(decode_response => DecodeResponse(response: *const ThinVec<u8>) -> *const nsIBinaryHttpResponse);
    fn decode_response(
        &self,
        response: &ThinVec<u8>,
    ) -> Result<RefPtr<nsIBinaryHttpResponse>, nsresult> {
        let mut cursor = Cursor::new(response);
        let decoded = Message::read_bhttp(&mut cursor).map_err(|_| NS_ERROR_UNEXPECTED)?;
        let status = decoded.control().status().ok_or(NS_ERROR_UNEXPECTED)?;
        let headers = decoded
            .header()
            .iter()
            .map(|field| (field.name().to_vec(), field.value().to_vec()))
            .collect();
        let content = decoded.content().to_vec();
        let binary_http_response = BinaryHttpResponse::allocate(InitBinaryHttpResponse {
            status,
            headers,
            content,
        });
        binary_http_response
            .query_interface::<nsIBinaryHttpResponse>()
            .ok_or(NS_ERROR_FAILURE)
    }

    xpcom_method!(decode_request => DecodeRequest(request: *const ThinVec<u8>) -> *const nsIBinaryHttpRequest);
    fn decode_request(
        &self,
        request: &ThinVec<u8>,
    ) -> Result<RefPtr<nsIBinaryHttpRequest>, nsresult> {
        let mut cursor = Cursor::new(request);
        let decoded = Message::read_bhttp(&mut cursor).map_err(|_| NS_ERROR_UNEXPECTED)?;
        let method = decoded
            .control()
            .method()
            .ok_or(NS_ERROR_UNEXPECTED)?
            .to_vec();
        let scheme = decoded
            .control()
            .scheme()
            .ok_or(NS_ERROR_UNEXPECTED)?
            .to_vec();
        // authority and path can be empty, in which case we return empty arrays
        let authority = decoded.control().authority().unwrap_or(&[]).to_vec();
        let path = decoded.control().path().unwrap_or(&[]).to_vec();
        let headers = decoded
            .header()
            .iter()
            .map(|field| (field.name().to_vec(), field.value().to_vec()))
            .collect();
        let content = decoded.content().to_vec();
        let binary_http_request = BinaryHttpRequest::allocate(InitBinaryHttpRequest {
            method,
            scheme,
            authority,
            path,
            headers,
            content,
        });
        binary_http_request
            .query_interface::<nsIBinaryHttpRequest>()
            .ok_or(NS_ERROR_FAILURE)
    }

    xpcom_method!(encode_response => EncodeResponse(response: *const nsIBinaryHttpResponse) -> ThinVec<u8>);
    fn encode_response(&self, response: &nsIBinaryHttpResponse) -> Result<ThinVec<u8>, nsresult> {
        let mut status = 0;
        unsafe { response.GetStatus(&mut status) }.to_result()?;
        let mut message = Message::response(status);
        let mut header_names = ThinVec::new();
        unsafe { response.GetHeaderNames(&mut header_names) }.to_result()?;
        let mut header_values = ThinVec::with_capacity(header_names.len());
        unsafe { response.GetHeaderValues(&mut header_values) }.to_result()?;
        if header_names.len() != header_values.len() {
            return Err(NS_ERROR_INVALID_ARG);
        }
        for (name, value) in header_names.iter().zip(header_values.iter()) {
            message.put_header(name.to_vec(), value.to_vec());
        }
        let mut content = ThinVec::new();
        unsafe { response.GetContent(&mut content) }.to_result()?;
        message.write_content(content);
        let mut encoded = ThinVec::new();
        message
            .write_bhttp(Mode::KnownLength, &mut encoded)
            .map_err(|_| NS_ERROR_FAILURE)?;
        Ok(encoded)
    }
}

#[no_mangle]
pub extern "C" fn binary_http_constructor(
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> nsresult {
    let binary_http = BinaryHttp::allocate(InitBinaryHttp {});
    unsafe { binary_http.QueryInterface(iid, result) }
}
