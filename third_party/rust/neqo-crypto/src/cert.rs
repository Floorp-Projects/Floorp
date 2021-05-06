// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::err::secstatus_to_res;
use crate::p11::{
    CERTCertList, CERTCertListNode, CERT_GetCertificateDer, CertList, PRCList, SECItem,
    SECItemArray, SECItemType,
};
use crate::ssl::{
    PRFileDesc, SSL_PeerCertificateChain, SSL_PeerSignedCertTimestamps,
    SSL_PeerStapledOCSPResponses,
};
use neqo_common::qerror;

use std::convert::TryFrom;
use std::ptr::{null_mut, NonNull};

use std::slice;

pub struct CertificateInfo {
    certs: CertList,
    cursor: *const CERTCertListNode,
    /// stapled_ocsp_responses and signed_cert_timestamp are properties
    /// associated with each of the certificates. Right now, NSS only
    /// reports the value for the end-entity certificate (the first).
    stapled_ocsp_responses: Option<Vec<Vec<u8>>>,
    signed_cert_timestamp: Option<Vec<u8>>,
}

fn peer_certificate_chain(fd: *mut PRFileDesc) -> Option<(CertList, *const CERTCertListNode)> {
    let chain = unsafe { SSL_PeerCertificateChain(fd) };
    let certs = match NonNull::new(chain.cast::<CERTCertList>()) {
        Some(certs_ptr) => CertList::new(certs_ptr),
        None => return None,
    };
    let cursor = CertificateInfo::head(&certs);
    Some((certs, cursor))
}

// As explained in rfc6961, an OCSPResponseList can have at most
// 2^24 items. Casting its length is therefore safe even on 32 bits targets.
fn stapled_ocsp_responses(fd: *mut PRFileDesc) -> Option<Vec<Vec<u8>>> {
    let ocsp_nss = unsafe { SSL_PeerStapledOCSPResponses(fd) };
    match NonNull::new(ocsp_nss as *mut SECItemArray) {
        Some(ocsp_ptr) => {
            let mut ocsp_helper: Vec<Vec<u8>> = Vec::new();
            let len = if let Ok(l) = isize::try_from(unsafe { ocsp_ptr.as_ref().len }) {
                l
            } else {
                qerror!([format!("{:p}", fd)], "Received illegal OSCP length");
                return None;
            };
            for idx in 0..len {
                let itemp: *const SECItem = unsafe { ocsp_ptr.as_ref().items.offset(idx).cast() };
                let item = unsafe { slice::from_raw_parts((*itemp).data, (*itemp).len as usize) };
                ocsp_helper.push(item.to_owned());
            }
            Some(ocsp_helper)
        }
        None => None,
    }
}

fn signed_cert_timestamp(fd: *mut PRFileDesc) -> Option<Vec<u8>> {
    let sct_nss = unsafe { SSL_PeerSignedCertTimestamps(fd) };
    match NonNull::new(sct_nss as *mut SECItem) {
        Some(sct_ptr) => {
            let sct_slice = unsafe {
                slice::from_raw_parts(sct_ptr.as_ref().data, sct_ptr.as_ref().len as usize)
            };
            Some(sct_slice.to_owned())
        }
        None => None,
    }
}

impl CertificateInfo {
    pub(crate) fn new(fd: *mut PRFileDesc) -> Option<Self> {
        match peer_certificate_chain(fd) {
            Some((certs, cursor)) => Some(Self {
                certs,
                cursor,
                stapled_ocsp_responses: stapled_ocsp_responses(fd),
                signed_cert_timestamp: signed_cert_timestamp(fd),
            }),
            None => None,
        }
    }

    fn head(certs: &CertList) -> *const CERTCertListNode {
        // Three stars: one for the reference, one for the wrapper, one to deference the pointer.
        unsafe { (&(***certs).list as *const PRCList).cast() }
    }
}

impl<'a> Iterator for &'a mut CertificateInfo {
    type Item = &'a [u8];
    fn next(&mut self) -> Option<&'a [u8]> {
        self.cursor = unsafe { *self.cursor }.links.next.cast();
        if self.cursor == CertificateInfo::head(&self.certs) {
            return None;
        }
        let mut item = SECItem {
            type_: SECItemType::siBuffer,
            data: null_mut(),
            len: 0,
        };
        let cert = unsafe { *self.cursor }.cert;
        secstatus_to_res(unsafe { CERT_GetCertificateDer(cert, &mut item) })
            .expect("getting DER from certificate should work");
        Some(unsafe { std::slice::from_raw_parts(item.data, item.len as usize) })
    }
}

impl CertificateInfo {
    pub fn stapled_ocsp_responses(&mut self) -> &Option<Vec<Vec<u8>>> {
        &self.stapled_ocsp_responses
    }

    pub fn signed_cert_timestamp(&mut self) -> &Option<Vec<u8>> {
        &self.signed_cert_timestamp
    }
}
