/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use pkcs11_bindings::nss::*;
use pkcs11_bindings::*;

use smallvec::SmallVec;

use crate::certdata::*;

// The token stores 2N+1 objects: one NSS root list object, N certificate objects, and N trust
// objects.
//
// Internally, the token identifies each object by its ObjectClass (RootList, Certificate,
// or Trust) and its index in the list of objects of the same class.
//
// The PKCS#11 interface, on the other hand, identifies each object with a unique, non-zero,
// unsigned long. This ulong is referred to as the object's CK_OBJECT_HANDLE.
//
// We're free to choose the mapping between ObjectHandles and CK_OBJECT_HANDLEs. Currently we
// encode the ObjectClass in the low 2 bits of the CK_OBJECT_HANDLE and the index in the higher
// bits. We use the values 1, 2, and 3 for ObjectClass to avoid using 0 as a CK_OBJECT_HANDLE.
//
#[derive(Clone, Copy)]
pub enum ObjectClass {
    RootList = 1,
    Certificate = 2,
    Trust = 3,
}

#[derive(Clone, Copy)]
pub struct ObjectHandle {
    class: ObjectClass,
    index: usize,
}

impl TryFrom<CK_OBJECT_HANDLE> for ObjectHandle {
    type Error = ();
    fn try_from(handle: CK_OBJECT_HANDLE) -> Result<Self, Self::Error> {
        if let Ok(handle) = usize::try_from(handle) {
            let index = handle >> 2;
            let class = match handle & 3 {
                1 if index == 0 => ObjectClass::RootList,
                2 if index < BUILTINS.len() => ObjectClass::Certificate,
                3 if index < BUILTINS.len() => ObjectClass::Trust,
                _ => return Err(()),
            };
            Ok(ObjectHandle { class, index })
        } else {
            Err(())
        }
    }
}

impl From<ObjectHandle> for CK_OBJECT_HANDLE {
    fn from(object_handle: ObjectHandle) -> CK_OBJECT_HANDLE {
        match CK_OBJECT_HANDLE::try_from(object_handle.index) {
            Ok(index) => (index << 2) | (object_handle.class as CK_OBJECT_HANDLE),
            Err(_) => 0,
        }
    }
}

pub fn get_attribute(attribute: CK_ATTRIBUTE_TYPE, object: &ObjectHandle) -> Option<&'static [u8]> {
    match object.class {
        ObjectClass::RootList => get_root_list_attribute(attribute),
        ObjectClass::Certificate => get_cert_attribute(attribute, &BUILTINS[object.index]),
        ObjectClass::Trust => get_trust_attribute(attribute, &BUILTINS[object.index]),
    }
}

// Every attribute that appears in certdata.txt must have a corresponding match arm in one of the
// get_*_attribute functions.
//
fn get_root_list_attribute(attribute: CK_ATTRIBUTE_TYPE) -> Option<&'static [u8]> {
    match attribute {
        CKA_CLASS => Some(CKO_NSS_BUILTIN_ROOT_LIST_BYTES),
        CKA_TOKEN => Some(CK_TRUE_BYTES),
        CKA_PRIVATE => Some(CK_FALSE_BYTES),
        CKA_MODIFIABLE => Some(CK_FALSE_BYTES),
        CKA_LABEL => Some(&ROOT_LIST_LABEL[..]),
        _ => None,
    }
}

fn get_cert_attribute(attribute: CK_ATTRIBUTE_TYPE, cert: &Root) -> Option<&[u8]> {
    match attribute {
        CKA_CLASS => Some(CKO_CERTIFICATE_BYTES),
        CKA_TOKEN => Some(CK_TRUE_BYTES),
        CKA_PRIVATE => Some(CK_FALSE_BYTES),
        CKA_MODIFIABLE => Some(CK_FALSE_BYTES),
        CKA_LABEL => Some(cert.label.as_bytes()),
        CKA_CERTIFICATE_TYPE => Some(CKC_X_509_BYTES),
        CKA_SUBJECT => Some(cert.der_name()),
        CKA_ID => Some(b"0\0"), // null terminated to match C implementation
        CKA_ISSUER => Some(cert.der_name()),
        CKA_SERIAL_NUMBER => Some(cert.der_serial()),
        CKA_VALUE => Some(cert.der_cert),
        CKA_NSS_MOZILLA_CA_POLICY => cert.mozilla_ca_policy,
        CKA_NSS_SERVER_DISTRUST_AFTER => cert.server_distrust_after,
        CKA_NSS_EMAIL_DISTRUST_AFTER => cert.email_distrust_after,
        _ => None,
    }
}

fn get_trust_attribute(attribute: CK_ATTRIBUTE_TYPE, cert: &Root) -> Option<&[u8]> {
    match attribute {
        CKA_CLASS => Some(CKO_NSS_TRUST_BYTES),
        CKA_TOKEN => Some(CK_TRUE_BYTES),
        CKA_PRIVATE => Some(CK_FALSE_BYTES),
        CKA_MODIFIABLE => Some(CK_FALSE_BYTES),
        CKA_LABEL => Some(cert.label.as_bytes()),
        CKA_CERT_SHA1_HASH => Some(&cert.sha1[..]),
        CKA_CERT_MD5_HASH => Some(&cert.md5[..]),
        CKA_ISSUER => Some(cert.der_name()),
        CKA_SERIAL_NUMBER => Some(cert.der_serial()),
        CKA_TRUST_STEP_UP_APPROVED => Some(CK_FALSE_BYTES),
        CKA_TRUST_SERVER_AUTH => Some(cert.trust_server),
        CKA_TRUST_EMAIL_PROTECTION => Some(cert.trust_email),
        CKA_TRUST_CODE_SIGNING => Some(CKT_NSS_MUST_VERIFY_TRUST_BYTES),
        _ => None,
    }
}

// A query matches an object if each term matches some attribute of the object. A search result is
// a list of object handles. Typical queries yield zero or one results, so we optimize for this
// case.
//
pub type Query<'a> = [(CK_ATTRIBUTE_TYPE, &'a [u8])];
pub type SearchResult = SmallVec<[ObjectHandle; 1]>;

pub fn search(query: &Query) -> SearchResult {
    // The BUILTINS list is sorted by name. So if the query includes a CKA_SUBJECT or CKA_ISSUER
    // field we can binary search.
    for &(attr, value) in query {
        if attr == CKA_SUBJECT || attr == CKA_ISSUER {
            return search_by_name(value, query);
        }
    }

    let mut results: SearchResult = SearchResult::default();

    // A query with no name term might match the root list object
    if match_root_list(query) {
        results.push(ObjectHandle {
            class: ObjectClass::RootList,
            index: 0,
        });
    }

    // A query with a CKA_CLASS term matches exactly one type of object, and we should avoid
    // iterating over BUILTINS when CKO_CLASS is neither CKO_CERTIFICATE_BYTES nor
    // CKO_NSS_TRUST_BYTES.
    let mut maybe_cert = true;
    let mut maybe_trust = true;
    for &(attr, value) in query {
        if attr == CKA_CLASS {
            maybe_cert = value.eq(CKO_CERTIFICATE_BYTES);
            maybe_trust = value.eq(CKO_NSS_TRUST_BYTES);
            break;
        }
    }

    if !(maybe_cert || maybe_trust) {
        return results; // The root list or nothing.
    }

    for (index, builtin) in BUILTINS.iter().enumerate() {
        if maybe_cert && match_cert(query, builtin) {
            results.push(ObjectHandle {
                class: ObjectClass::Certificate,
                index,
            });
        }
        if maybe_trust && match_trust(query, builtin) {
            results.push(ObjectHandle {
                class: ObjectClass::Trust,
                index,
            });
        }
    }
    results
}

fn search_by_name(name: &[u8], query: &Query) -> SearchResult {
    let mut results: SearchResult = SearchResult::default();

    let index = match BUILTINS.binary_search_by_key(&name, |r| r.der_name()) {
        Ok(index) => index,
        _ => return results,
    };

    // binary search returned a matching index, but maybe not the smallest
    let mut min = index;
    while min > 0 && name.eq(BUILTINS[min - 1].der_name()) {
        min -= 1;
    }

    // ... and maybe not the largest.
    let mut max = index;
    while max < BUILTINS.len() - 1 && name.eq(BUILTINS[max + 1].der_name()) {
        max += 1;
    }

    for (index, builtin) in BUILTINS.iter().enumerate().take(max + 1).skip(min) {
        if match_cert(query, builtin) {
            results.push(ObjectHandle {
                class: ObjectClass::Certificate,
                index,
            });
        }
        if match_trust(query, builtin) {
            results.push(ObjectHandle {
                class: ObjectClass::Trust,
                index,
            });
        }
    }

    results
}

fn match_root_list(query: &Query) -> bool {
    for &(typ, x) in query {
        match get_root_list_attribute(typ) {
            Some(y) if x.eq(y) => (),
            _ => return false,
        }
    }
    true
}

fn match_cert(query: &Query, cert: &Root) -> bool {
    for &(typ, x) in query {
        match get_cert_attribute(typ, cert) {
            Some(y) if x.eq(y) => (),
            _ => return false,
        }
    }
    true
}

fn match_trust(query: &Query, cert: &Root) -> bool {
    for &(typ, x) in query {
        match get_trust_attribute(typ, cert) {
            Some(y) if x.eq(y) => (),
            _ => return false,
        }
    }
    true
}

#[cfg(test)]
mod internal_tests {
    use crate::certdata::BUILTINS;
    use crate::internal::*;
    use pkcs11_bindings::*;

    // commented out to avoid vendoring x509_parser
    // fn is_valid_utctime(utctime: &[u8]) -> bool {
    //     /* TODO: actual validation */
    //     utctime.len() == 13
    // }
    // #[test]
    // fn test_certdata() {
    //     for root in BUILTINS {
    //         // the der_cert field is valid DER
    //         let parsed_cert = X509Certificate::from_der(root.der_cert);
    //         assert!(parsed_cert.is_ok());

    //         // the der_cert field has no trailing data
    //         let (trailing, parsed_cert) = parsed_cert.unwrap();
    //         assert!(trailing.is_empty());

    //         // the der_serial field matches the encoded serial
    //         assert!(root.der_serial.len() > 2);
    //         assert!(root.der_serial[0] == 0x02); // der integer
    //         assert!(root.der_serial[1] <= 20); // no more than 20 bytes long
    //         assert!(root.der_serial[1] as usize == root.der_serial.len() - 2);
    //         assert!(parsed_cert.raw_serial().eq(&root.der_serial[2..]));

    //         // the der_name field matches the encoded subject
    //         assert!(parsed_cert.subject.as_raw().eq(root.der_name));

    //         // the der_name field matches the encoded issuer
    //         assert!(parsed_cert.issuer.as_raw().eq(root.der_name));

    //         // The server_distrust_after field is None or a valid UTC time
    //         if let Some(utctime) = root.server_distrust_after {
    //             assert!(is_valid_utctime(&utctime));
    //         }

    //         // The email_distrust_after field is None or a valid UTC time
    //         if let Some(utctime) = root.email_distrust_after {
    //             assert!(is_valid_utctime(&utctime));
    //         }

    //         assert!(
    //             root.trust_server == CKT_NSS_MUST_VERIFY_TRUST_BYTES
    //                 || root.trust_server == CKT_NSS_TRUSTED_DELEGATOR_BYTES
    //                 || root.trust_server == CKT_NSS_NOT_TRUSTED_BYTES
    //         );
    //         assert!(
    //             root.trust_email == CKT_NSS_MUST_VERIFY_TRUST_BYTES
    //                 || root.trust_email == CKT_NSS_TRUSTED_DELEGATOR_BYTES
    //                 || root.trust_email == CKT_NSS_NOT_TRUSTED_BYTES
    //         );
    //     }
    // }

    #[test]
    fn test_builtins_sorted() {
        for i in 0..(BUILTINS.len() - 1) {
            assert!(BUILTINS[i].der_name.le(BUILTINS[i + 1].der_name));
        }
    }

    #[test]
    fn test_search() {
        // search for an element that will not be found
        let result = search(&[(CKA_TOKEN, &[CK_FALSE])]);
        assert_eq!(result.len(), 0);

        // search for root list
        let result = search(&[(CKA_CLASS, CKO_NSS_BUILTIN_ROOT_LIST_BYTES)]);
        assert!(result.len() == 1);

        // search by name
        let result = search(&[
            (CKA_CLASS, CKO_CERTIFICATE_BYTES),
            (CKA_SUBJECT, BUILTINS[0].der_name),
        ]);
        assert!(result.len() >= 1);

        // search by issuer and serial
        let result = search(&[
            (CKA_ISSUER, BUILTINS[0].der_name),
            (CKA_SERIAL_NUMBER, BUILTINS[0].der_serial),
        ]);
        assert!(result.len() >= 1);
    }
}
