/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nserror;
extern crate ohttp;
extern crate rand;
extern crate thin_vec;
#[macro_use]
extern crate xpcom;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_NOT_AVAILABLE, NS_OK};
use ohttp::hpke::{Aead, Kdf, Kem};
use ohttp::{
    ClientRequest, ClientResponse, KeyConfig, KeyId, Server, ServerResponse, SymmetricSuite,
};
use thin_vec::ThinVec;
use xpcom::interfaces::{
    nsIObliviousHttpClientRequest, nsIObliviousHttpClientResponse, nsIObliviousHttpServer,
    nsIObliviousHttpServerResponse,
};
use xpcom::{xpcom_method, RefPtr};

use std::cell::RefCell;

#[xpcom(implement(nsIObliviousHttpClientResponse), atomic)]
struct ObliviousHttpClientResponse {
    response: RefCell<Option<ClientResponse>>,
}

impl ObliviousHttpClientResponse {
    xpcom_method!(decapsulate => Decapsulate(enc_response: *const ThinVec<u8>) -> ThinVec<u8>);
    fn decapsulate(&self, enc_response: &ThinVec<u8>) -> Result<ThinVec<u8>, nsresult> {
        let response = self
            .response
            .borrow_mut()
            .take()
            .ok_or(NS_ERROR_NOT_AVAILABLE)?;
        let decapsulated = response
            .decapsulate(enc_response)
            .map_err(|_| NS_ERROR_FAILURE)?;
        Ok(decapsulated.into_iter().collect())
    }
}

#[xpcom(implement(nsIObliviousHttpClientRequest), atomic)]
struct ObliviousHttpClientRequest {
    enc_request: Vec<u8>,
    response: RefPtr<nsIObliviousHttpClientResponse>,
}

impl ObliviousHttpClientRequest {
    xpcom_method!(get_enc_request => GetEncRequest() -> ThinVec<u8>);
    fn get_enc_request(&self) -> Result<ThinVec<u8>, nsresult> {
        Ok(self.enc_request.clone().into_iter().collect())
    }

    xpcom_method!(get_response => GetResponse() -> *const nsIObliviousHttpClientResponse);
    fn get_response(&self) -> Result<RefPtr<nsIObliviousHttpClientResponse>, nsresult> {
        Ok(self.response.clone())
    }
}

#[xpcom(implement(nsIObliviousHttpServerResponse), atomic)]
struct ObliviousHttpServerResponse {
    request: Vec<u8>,
    server_response: RefCell<Option<ServerResponse>>,
}

impl ObliviousHttpServerResponse {
    xpcom_method!(get_request => GetRequest() -> ThinVec<u8>);
    fn get_request(&self) -> Result<ThinVec<u8>, nsresult> {
        Ok(self.request.clone().into_iter().collect())
    }

    xpcom_method!(encapsulate => Encapsulate(response: *const ThinVec<u8>) -> ThinVec<u8>);
    fn encapsulate(&self, response: &ThinVec<u8>) -> Result<ThinVec<u8>, nsresult> {
        let server_response = self
            .server_response
            .borrow_mut()
            .take()
            .ok_or(NS_ERROR_NOT_AVAILABLE)?;
        Ok(server_response
            .encapsulate(response)
            .map_err(|_| NS_ERROR_FAILURE)?
            .into_iter()
            .collect())
    }
}

#[xpcom(implement(nsIObliviousHttpServer), atomic)]
struct ObliviousHttpServer {
    server: RefCell<Server>,
}

impl ObliviousHttpServer {
    xpcom_method!(get_encoded_config => GetEncodedConfig() -> ThinVec<u8>);
    fn get_encoded_config(&self) -> Result<ThinVec<u8>, nsresult> {
        let server = self.server.borrow_mut();
        Ok(server
            .config()
            .encode()
            .map_err(|_| NS_ERROR_FAILURE)?
            .into_iter()
            .collect())
    }

    xpcom_method!(decapsulate => Decapsulate(enc_request: *const ThinVec<u8>) -> *const nsIObliviousHttpServerResponse);
    fn decapsulate(
        &self,
        enc_request: &ThinVec<u8>,
    ) -> Result<RefPtr<nsIObliviousHttpServerResponse>, nsresult> {
        let mut server = self.server.borrow_mut();
        let (request, server_response) = server
            .decapsulate(enc_request)
            .map_err(|_| NS_ERROR_FAILURE)?;
        let oblivious_http_server_response =
            ObliviousHttpServerResponse::allocate(InitObliviousHttpServerResponse {
                request,
                server_response: RefCell::new(Some(server_response)),
            });
        oblivious_http_server_response
            .query_interface::<nsIObliviousHttpServerResponse>()
            .ok_or(NS_ERROR_FAILURE)
    }
}

#[xpcom(implement(nsIObliviousHttp), atomic)]
struct ObliviousHttp {}

impl ObliviousHttp {
    xpcom_method!(encapsulate_request => EncapsulateRequest(encoded_config: *const ThinVec<u8>,
    request: *const ThinVec<u8>) -> *const nsIObliviousHttpClientRequest);
    fn encapsulate_request(
        &self,
        encoded_config: &ThinVec<u8>,
        request: &ThinVec<u8>,
    ) -> Result<RefPtr<nsIObliviousHttpClientRequest>, nsresult> {
        ohttp::init();

        let client = ClientRequest::new(encoded_config).map_err(|_| NS_ERROR_FAILURE)?;
        let (enc_request, response) = client.encapsulate(request).map_err(|_| NS_ERROR_FAILURE)?;
        let oblivious_http_client_response =
            ObliviousHttpClientResponse::allocate(InitObliviousHttpClientResponse {
                response: RefCell::new(Some(response)),
            });
        let response = oblivious_http_client_response
            .query_interface::<nsIObliviousHttpClientResponse>()
            .ok_or(NS_ERROR_FAILURE)?;
        let oblivious_http_client_request =
            ObliviousHttpClientRequest::allocate(InitObliviousHttpClientRequest {
                enc_request,
                response,
            });
        oblivious_http_client_request
            .query_interface::<nsIObliviousHttpClientRequest>()
            .ok_or(NS_ERROR_FAILURE)
    }

    xpcom_method!(server => Server() -> *const nsIObliviousHttpServer);
    fn server(&self) -> Result<RefPtr<nsIObliviousHttpServer>, nsresult> {
        ohttp::init();

        let key_id: KeyId = rand::random::<u8>();
        let kem: Kem = Kem::X25519Sha256;
        let symmetric = vec![
            SymmetricSuite::new(Kdf::HkdfSha256, Aead::Aes128Gcm),
            SymmetricSuite::new(Kdf::HkdfSha256, Aead::ChaCha20Poly1305),
        ];
        let key_config = KeyConfig::new(key_id, kem, symmetric).map_err(|_| NS_ERROR_FAILURE)?;
        let server = Server::new(key_config).map_err(|_| NS_ERROR_FAILURE)?;
        let oblivious_http_server = ObliviousHttpServer::allocate(InitObliviousHttpServer {
            server: RefCell::new(server),
        });
        oblivious_http_server
            .query_interface::<nsIObliviousHttpServer>()
            .ok_or(NS_ERROR_FAILURE)
    }
}

#[no_mangle]
pub extern "C" fn oblivious_http_constructor(
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> nserror::nsresult {
    let oblivious_http = ObliviousHttp::allocate(InitObliviousHttp {});
    unsafe { oblivious_http.QueryInterface(iid, result) }
}
