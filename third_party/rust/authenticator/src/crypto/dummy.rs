use super::{ByteBuf, COSEKey, ECDHSecret, ECDSACurve};
use serde::Serialize;
/*
This is a dummy implementation for CI, to avoid having to install NSS or openSSL in the CI-pipeline
*/

pub type Result<T> = std::result::Result<T, BackendError>;

#[derive(Clone, Debug, PartialEq, Serialize)]
pub enum BackendError {}

pub(crate) fn serialize_key(_curve: ECDSACurve, key: &[u8]) -> Result<(ByteBuf, ByteBuf)> {
    // Copy from NSS
    let length = key[1..].len() / 2;
    let chunks: Vec<_> = key[1..].chunks_exact(length).collect();
    Ok((
        ByteBuf::from(chunks[0].to_vec()),
        ByteBuf::from(chunks[1].to_vec()),
    ))
}

pub(crate) fn encapsulate(_key: &COSEKey) -> Result<ECDHSecret> {
    unimplemented!()
}

pub(crate) fn encrypt(
    _key: &[u8],
    _plain_text: &[u8], /*PlainText*/
) -> Result<Vec<u8> /*CypherText*/> {
    unimplemented!()
}

pub(crate) fn decrypt(
    _key: &[u8],
    _cypher_text: &[u8], /*CypherText*/
) -> Result<Vec<u8> /*PlainText*/> {
    unimplemented!()
}

pub(crate) fn authenticate(_token: &[u8], _input: &[u8]) -> Result<Vec<u8>> {
    unimplemented!()
}
