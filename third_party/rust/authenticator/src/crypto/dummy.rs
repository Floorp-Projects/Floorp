use super::CryptoError;

/*
This is a dummy implementation for CI, to avoid having to install NSS or openSSL in the CI-pipeline
*/

pub type Result<T> = std::result::Result<T, CryptoError>;

pub fn ecdhe_p256_raw(_peer_spki: &[u8]) -> Result<(Vec<u8>, Vec<u8>)> {
    unimplemented!()
}

pub fn encrypt_aes_256_cbc_no_pad(
    _key: &[u8],
    _iv: Option<&[u8]>,
    _data: &[u8],
) -> Result<Vec<u8>> {
    unimplemented!()
}

pub fn decrypt_aes_256_cbc_no_pad(
    _key: &[u8],
    _iv: Option<&[u8]>,
    _data: &[u8],
) -> Result<Vec<u8>> {
    unimplemented!()
}

pub fn hmac_sha256(_key: &[u8], _data: &[u8]) -> Result<Vec<u8>> {
    unimplemented!()
}

pub fn sha256(_data: &[u8]) -> Result<Vec<u8>> {
    unimplemented!()
}

pub fn random_bytes(_count: usize) -> Result<Vec<u8>> {
    unimplemented!()
}
