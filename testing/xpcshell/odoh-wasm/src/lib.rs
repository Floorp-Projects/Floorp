use hpke::{
    kem::X25519HkdfSha256,
    Kem as KemTrait, Serializable,
};

use odoh_rs::protocol::{
    create_response_msg, parse_received_query,
    ObliviousDoHConfigContents, ObliviousDoHKeyPair,
    ObliviousDoHQueryBody,
};

use futures::executor;
use hex;
use wasm_bindgen::prelude::*;

pub type Kem = X25519HkdfSha256;

// When the `wee_alloc` feature is enabled, use `wee_alloc` as the global
// allocator.
#[cfg(feature = "wee_alloc")]
#[global_allocator]
static ALLOC: wee_alloc::WeeAlloc = wee_alloc::WeeAlloc::INIT;

pub const ODOH_VERSION: u16 = 0x0001;
const KEM_ID: u16 = 0x0020;
const KDF_ID: u16 = 0x0001;
const AEAD_ID: u16 = 0x0001;

// random bytes, should be 32 bytes for X25519 keys
pub const IKM: &str = "871389a8727130974e3eb3ee528d440a871389a8727130974e3eb3ee528d440a";

#[wasm_bindgen]
extern "C" {
    // Use `js_namespace` here to bind `console.log(..)` instead of just
    // `log(..)`
    #[wasm_bindgen(js_namespace = console)]
    fn log(s: &str);

    // The `console.log` is quite polymorphic, so we can bind it with multiple
    // signatures. Note that we need to use `js_name` to ensure we always call
    // `log` in JS.
    #[wasm_bindgen(js_namespace = console, js_name = log)]
    fn log_u32(a: u32);

    // Multiple arguments too!
    #[wasm_bindgen(js_namespace = console, js_name = log)]
    fn log_many(a: &str, b: &str);
}

macro_rules! console_log {
    // Note that this is using the `log` function imported above during
    // `bare_bones`
    ($($t:tt)*) => (log(&format_args!($($t)*).to_string()))
}

fn generate_key_pair() -> ObliviousDoHKeyPair {
    let ikm_bytes = hex::decode(IKM).unwrap();
    let (secret_key, public_key) = Kem::derive_keypair(&ikm_bytes);
    let public_key_bytes = public_key.to_bytes().to_vec();
    let odoh_public_key = ObliviousDoHConfigContents {
        kem_id: KEM_ID,
        kdf_id: KDF_ID,
        aead_id: AEAD_ID,
        public_key: public_key_bytes,
    };
    ObliviousDoHKeyPair {
        private_key: secret_key,
        public_key: odoh_public_key,
    }
}

#[wasm_bindgen]
pub fn get_odoh_config() -> js_sys::Uint8Array {
    let key_pair = generate_key_pair();
    let public_key_bytes = key_pair.public_key.public_key;
    let length_bytes = (public_key_bytes.len() as u16).to_be_bytes();
    let odoh_config_length = 12 + public_key_bytes.len();
    let version = ODOH_VERSION;
    let odoh_contents_length = 8 + public_key_bytes.len();
    let kem_id = KEM_ID; // DHKEM(X25519, HKDF-SHA256)
    let kdf_id = KDF_ID; // KDF(SHA-256)
    let aead_id = AEAD_ID; // AEAD(AES-GCM-128)
    let mut result = vec![];
    result.extend(&((odoh_config_length as u16).to_be_bytes()));
    result.extend(&((version as u16).to_be_bytes()));
    result.extend(&((odoh_contents_length as u16).to_be_bytes()));
    result.extend(&((kem_id as u16).to_be_bytes()));
    result.extend(&((kdf_id as u16).to_be_bytes()));
    result.extend(&((aead_id as u16).to_be_bytes()));
    result.extend(&length_bytes);
    result.extend(&public_key_bytes);
    return js_sys::Uint8Array::from(&result[..]);
}

static mut QUERY_BODY: Option<ObliviousDoHQueryBody> = None;
static mut SERVER_SECRET: Option<Vec<u8>> = None;

#[wasm_bindgen]
pub fn decrypt_query(
    odoh_encrypted_query_msg: &[u8],
) -> js_sys::Uint8Array {
    let mut result = vec![];
    unsafe {
    let key_pair = generate_key_pair();
    let parsed_res =
        executor::block_on(parse_received_query(&key_pair, &odoh_encrypted_query_msg));
    let (parsed_query, secret) = match parsed_res {
        Ok(t)  => (t.0, t.1),
        Err(_) => {
            console_log!("parse_received_query failed!");
            return js_sys::Uint8Array::new_with_length(0)
        },
    };

    result.extend(&parsed_query.dns_msg);

        QUERY_BODY = Some(parsed_query);
        SERVER_SECRET = Some(secret);
    }

    return js_sys::Uint8Array::from(&result[..]);
}

#[wasm_bindgen]
pub fn create_response(
    response: &[u8],
) -> js_sys::Uint8Array {
    unsafe {
        if let Some(body) = &QUERY_BODY {
            if let Some(secret) = &SERVER_SECRET {
                // random bytes
                let nonce = vec![0x1b, 0xff, 0xfd, 0xff, 0x1a, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 0xe];
                let result = executor::block_on(create_response_msg(
                    &secret,
                    &response,
                    None,
                    Some(nonce),
                    &body,
                ));
                let generated_response = match result {
                    Ok(r) => r,
                    Err(_) => {
                        console_log!("create_response_msg failed!");
                        return js_sys::Uint8Array::new_with_length(0);
                    }
                };

                QUERY_BODY = None;
                SERVER_SECRET = None;
                return js_sys::Uint8Array::from(&generated_response[..]);
            }
        }
    }

    console_log!("create_response_msg failed!");
    return js_sys::Uint8Array::new_with_length(0);
}
