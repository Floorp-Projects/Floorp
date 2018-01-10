use cbor::CborType;

/// Sig_structure is a CBOR array:
///
/// Sig_structure = [
///   context : "Signature" / "Signature1" / "CounterSignature",
///   body_protected : empty_or_serialized_map,
///   ? sign_protected : empty_or_serialized_map,
///   external_aad : bstr,
///   payload : bstr
/// ]
///
/// In this case, the context is "Signature". There is no external_aad, so this defaults to a
/// zero-length bstr.
pub fn get_sig_struct_bytes(
    protected_body_header_serialized: CborType,
    protected_signature_header_serialized: CborType,
    payload: &[u8],
) -> Vec<u8> {
    let sig_structure_array: Vec<CborType> = vec![CborType::String(String::from("Signature")),
                                                  protected_body_header_serialized,
                                                  protected_signature_header_serialized,
                                                  CborType::Null,
                                                  CborType::Bytes(payload.to_vec())];

    CborType::Array(sig_structure_array).serialize()
}
