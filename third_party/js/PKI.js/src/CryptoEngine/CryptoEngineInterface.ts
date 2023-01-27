import type * as asn1js from "asn1js";
import type { AlgorithmIdentifier } from "../AlgorithmIdentifier";
import type { EncryptedContentInfo } from "../EncryptedContentInfo";
import type { PublicKeyInfo } from "../PublicKeyInfo";

export type CryptoEngineAlgorithmOperation = "sign" | "encrypt" | "generateKey" | "importKey" | "exportKey" | "verify";

/**
 * Algorithm parameters
 */
export interface CryptoEngineAlgorithmParams {
  /**
   * Algorithm
   */
  algorithm: Algorithm | object;
  /**
   * Key usages
   */
  usages: KeyUsage[];
}

export interface CryptoEngineSignatureParams {
  signatureAlgorithm: AlgorithmIdentifier;
  parameters: CryptoEngineAlgorithmParams;
}

export interface CryptoEngineSignWithPrivateKeyParams {
  algorithm: Algorithm;
}

/**
 * Public key parameters
 */
export interface CryptoEnginePublicKeyParams {
  /**
   * Algorithm
   */
  algorithm: CryptoEngineAlgorithmParams;
}


export type ContentEncryptionAesCbcParams = AesCbcParams & AesDerivedKeyParams;
export type ContentEncryptionAesGcmParams = AesGcmParams & AesDerivedKeyParams;
export type ContentEncryptionAlgorithm = ContentEncryptionAesCbcParams | ContentEncryptionAesGcmParams;

export interface CryptoEngineEncryptParams {
  password: ArrayBuffer;
  contentEncryptionAlgorithm: ContentEncryptionAlgorithm;
  hmacHashAlgorithm: string;
  iterationCount: number;
  contentToEncrypt: ArrayBuffer;
  contentType: string;
}

export interface CryptoEngineDecryptParams {
  password: ArrayBuffer;
  encryptedContentInfo: EncryptedContentInfo;
}

export interface CryptoEngineStampDataWithPasswordParams {
  password: ArrayBuffer;
  hashAlgorithm: string;
  salt: ArrayBuffer;
  iterationCount: number;
  contentToStamp: ArrayBuffer;
}

export interface CryptoEngineVerifyDataStampedWithPasswordParams {
  password: ArrayBuffer;
  hashAlgorithm: string;
  salt: ArrayBuffer;
  iterationCount: number;
  contentToVerify: ArrayBuffer;
  signatureToVerify: ArrayBuffer;
}

export interface ICryptoEngine extends SubtleCrypto {
  name: string;
  crypto: Crypto;
  subtle: SubtleCrypto;

  getRandomValues<T extends ArrayBufferView | null>(array: T): T;

  /**
   * Get OID for each specific algorithm
   * @param algorithm WebCrypto Algorithm
   * @param safety If `true` throws exception on unknown algorithm. Default is `false`
   * @param target Name of the target
   * @throws Throws {@link Error} exception if unknown WebCrypto algorithm
   */
  getOIDByAlgorithm(algorithm: Algorithm, safety?: boolean, target?: string): string;
  /**
   * Get default algorithm parameters for each kind of operation
   * @param algorithmName Algorithm name to get common parameters for
   * @param operation Kind of operation: "sign", "encrypt", "generateKey", "importKey", "exportKey", "verify"
   */
  // TODO Use safety
  getAlgorithmParameters(algorithmName: string, operation: CryptoEngineAlgorithmOperation): CryptoEngineAlgorithmParams;

  /**
   * Gets WebCrypto algorithm by wel-known OID
   * @param oid algorithm identifier
   * @param safety if `true` throws exception on unknown algorithm identifier
   * @param target name of the target
   * @returns Returns WebCrypto algorithm or an empty object
   */
  getAlgorithmByOID<T extends Algorithm = Algorithm>(oid: string, safety?: boolean, target?: string): T | object;
  /**
   * Gets WebCrypto algorithm by wel-known OID
   * @param oid algorithm identifier
   * @param safety if `true` throws exception on unknown algorithm identifier
   * @param target name of the target
   * @returns Returns WebCrypto algorithm
   * @throws Throws {@link Error} exception if unknown algorithm identifier
   */
  getAlgorithmByOID<T extends Algorithm = Algorithm>(oid: string, safety: true, target?: string): T;

  /**
   * Getting hash algorithm by signature algorithm
   * @param signatureAlgorithm Signature algorithm
   */
  // TODO use safety
  getHashAlgorithm(signatureAlgorithm: AlgorithmIdentifier): string;

  /**
   * Get signature parameters by analyzing private key algorithm
   * @param privateKey The private key user would like to use
   * @param hashAlgorithm Hash algorithm user would like to use. Default is SHA-1
   */
  getSignatureParameters(privateKey: CryptoKey, hashAlgorithm?: string): Promise<CryptoEngineSignatureParams>;

  /**
   * Sign data with pre-defined private key
   * @param data Data to be signed
   * @param privateKey Private key to use
   * @param parameters Parameters for used algorithm
   */
  signWithPrivateKey(data: BufferSource, privateKey: CryptoKey, parameters: CryptoEngineSignWithPrivateKeyParams): Promise<ArrayBuffer>;

  /**
   * Verify data with the public key
   * @param data Data to be verified
   * @param signature Signature value
   * @param publicKeyInfo Public key information
   * @param signatureAlgorithm Signature algorithm
   * @param shaAlgorithm Hash algorithm
   */
  verifyWithPublicKey(data: BufferSource, signature: asn1js.BitString | asn1js.OctetString, publicKeyInfo: PublicKeyInfo, signatureAlgorithm: AlgorithmIdentifier, shaAlgorithm?: string): Promise<boolean>;

  getPublicKey(publicKeyInfo: PublicKeyInfo, signatureAlgorithm: AlgorithmIdentifier, parameters?: CryptoEnginePublicKeyParams): Promise<CryptoKey>;

  /**
   * Specialized function encrypting "EncryptedContentInfo" object using parameters
   * @param parameters
   */
  encryptEncryptedContentInfo(parameters: CryptoEngineEncryptParams): Promise<EncryptedContentInfo>;

  /**
   * Decrypt data stored in "EncryptedContentInfo" object using parameters
   * @param parameters
   */
  decryptEncryptedContentInfo(parameters: CryptoEngineDecryptParams): Promise<ArrayBuffer>;

  /**
   * Stamping (signing) data using algorithm similar to HMAC
   * @param parameters
   */
  stampDataWithPassword(parameters: CryptoEngineStampDataWithPasswordParams): Promise<ArrayBuffer>;
  verifyDataStampedWithPassword(parameters: CryptoEngineVerifyDataStampedWithPasswordParams): Promise<boolean>;
}

export interface CryptoEngineParameters {
  name?: string;
  crypto: Crypto;
  /**
   * @deprecated
   */
  subtle?: SubtleCrypto;
}

export interface CryptoEngineConstructor {
  new(params: CryptoEngineParameters): ICryptoEngine;
}
