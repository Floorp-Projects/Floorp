import { BitString, OctetString } from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier } from "../AlgorithmIdentifier";
import { EMPTY_STRING } from "../constants";
import { EncryptedContentInfo } from "../EncryptedContentInfo";
import { PublicKeyInfo } from "../PublicKeyInfo";
import * as type from "./CryptoEngineInterface";

export abstract class AbstractCryptoEngine implements type.ICryptoEngine {
  public name: string;
  public crypto: Crypto;
  public subtle: SubtleCrypto;

  /**
   * Constructor for CryptoEngine class
   * @param parameters
   */
  constructor(parameters: type.CryptoEngineParameters) {
    this.crypto = parameters.crypto;
    this.subtle = "webkitSubtle" in parameters.crypto
      ? (parameters.crypto as any).webkitSubtle
      : parameters.crypto.subtle;
    this.name = pvutils.getParametersValue(parameters, "name", EMPTY_STRING);
  }

  public abstract getOIDByAlgorithm(algorithm: Algorithm, safety?: boolean, target?: string): string;
  public abstract getAlgorithmParameters(algorithmName: string, operation: type.CryptoEngineAlgorithmOperation): type.CryptoEngineAlgorithmParams;
  public abstract getAlgorithmByOID<T extends Algorithm = Algorithm>(oid: string, safety?: boolean, target?: string): object | T;
  public abstract getAlgorithmByOID<T extends Algorithm = Algorithm>(oid: string, safety: true, target?: string): T;
  public abstract getAlgorithmByOID(oid: any, safety?: any, target?: any): object;
  public abstract getHashAlgorithm(signatureAlgorithm: AlgorithmIdentifier): string;
  public abstract getSignatureParameters(privateKey: CryptoKey, hashAlgorithm?: string): Promise<type.CryptoEngineSignatureParams>;
  public abstract signWithPrivateKey(data: BufferSource, privateKey: CryptoKey, parameters: type.CryptoEngineSignWithPrivateKeyParams): Promise<ArrayBuffer>;
  public abstract verifyWithPublicKey(data: BufferSource, signature: BitString | OctetString, publicKeyInfo: PublicKeyInfo, signatureAlgorithm: AlgorithmIdentifier, shaAlgorithm?: string): Promise<boolean>;
  public abstract getPublicKey(publicKeyInfo: PublicKeyInfo, signatureAlgorithm: AlgorithmIdentifier, parameters?: type.CryptoEnginePublicKeyParams): Promise<CryptoKey>;
  public abstract encryptEncryptedContentInfo(parameters: type.CryptoEngineEncryptParams): Promise<EncryptedContentInfo>;
  public abstract decryptEncryptedContentInfo(parameters: type.CryptoEngineDecryptParams): Promise<ArrayBuffer>;
  public abstract stampDataWithPassword(parameters: type.CryptoEngineStampDataWithPasswordParams): Promise<ArrayBuffer>;
  public abstract verifyDataStampedWithPassword(parameters: type.CryptoEngineVerifyDataStampedWithPasswordParams): Promise<boolean>;
  public async encrypt(algorithm: globalThis.AlgorithmIdentifier | RsaOaepParams | AesCtrParams | AesCbcParams | AesGcmParams, key: CryptoKey, data: BufferSource): Promise<ArrayBuffer>;
  public async encrypt(...args: any[]): Promise<ArrayBuffer> {
    return (this.subtle.encrypt as any)(...args);
  }

  public decrypt(algorithm: globalThis.AlgorithmIdentifier | RsaOaepParams | AesCtrParams | AesCbcParams | AesGcmParams, key: CryptoKey, data: BufferSource): Promise<ArrayBuffer>;
  public async decrypt(...args: any[]): Promise<ArrayBuffer> {
    return (this.subtle.decrypt as any)(...args);
  }

  public sign(algorithm: globalThis.AlgorithmIdentifier | RsaPssParams | EcdsaParams, key: CryptoKey, data: BufferSource): Promise<ArrayBuffer>;
  public sign(...args: any[]): Promise<ArrayBuffer> {
    return (this.subtle.sign as any)(...args);
  }

  public verify(algorithm: globalThis.AlgorithmIdentifier | RsaPssParams | EcdsaParams, key: CryptoKey, signature: BufferSource, data: BufferSource): Promise<boolean>;
  public async verify(...args: any[]): Promise<boolean> {
    return (this.subtle.verify as any)(...args);
  }

  public digest(algorithm: globalThis.AlgorithmIdentifier, data: BufferSource): Promise<ArrayBuffer>;
  public async digest(...args: any[]) {
    return (this.subtle.digest as any)(...args);
  }

  public generateKey(algorithm: RsaHashedKeyGenParams | EcKeyGenParams, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKeyPair>;
  public generateKey(algorithm: AesKeyGenParams | HmacKeyGenParams | Pbkdf2Params, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKey>;
  public generateKey(algorithm: globalThis.AlgorithmIdentifier, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKeyPair | CryptoKey>;
  public async generateKey(...args: any[]): Promise<CryptoKey | CryptoKeyPair> {
    return (this.subtle.generateKey as any)(...args);
  }

  public deriveKey(algorithm: globalThis.AlgorithmIdentifier | EcdhKeyDeriveParams | HkdfParams | Pbkdf2Params, baseKey: CryptoKey, derivedKeyType: globalThis.AlgorithmIdentifier | HkdfParams | Pbkdf2Params | AesDerivedKeyParams | HmacImportParams, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKey>;
  public deriveKey(algorithm: globalThis.AlgorithmIdentifier | EcdhKeyDeriveParams | HkdfParams | Pbkdf2Params, baseKey: CryptoKey, derivedKeyType: globalThis.AlgorithmIdentifier | HkdfParams | Pbkdf2Params | AesDerivedKeyParams | HmacImportParams, extractable: boolean, keyUsages: Iterable<KeyUsage>): Promise<CryptoKey>;
  public async deriveKey(...args: any[]): Promise<CryptoKey> {
    return (this.subtle.deriveKey as any)(...args);
  }

  public deriveBits(algorithm: globalThis.AlgorithmIdentifier | EcdhKeyDeriveParams | HkdfParams | Pbkdf2Params, baseKey: CryptoKey, length: number): Promise<ArrayBuffer>;
  public async deriveBits(...args: any[]): Promise<ArrayBuffer> {
    return (this.subtle.deriveBits as any)(...args);
  }

  public wrapKey(format: KeyFormat, key: CryptoKey, wrappingKey: CryptoKey, wrapAlgorithm: globalThis.AlgorithmIdentifier | RsaOaepParams | AesCtrParams | AesCbcParams | AesGcmParams): Promise<ArrayBuffer>;
  public async wrapKey(...args: any[]): Promise<ArrayBuffer> {
    return (this.subtle.wrapKey as any)(...args);
  }

  public unwrapKey(format: KeyFormat, wrappedKey: BufferSource, unwrappingKey: CryptoKey, unwrapAlgorithm: globalThis.AlgorithmIdentifier | RsaOaepParams | AesCtrParams | AesCbcParams | AesGcmParams, unwrappedKeyAlgorithm: globalThis.AlgorithmIdentifier | HmacImportParams | RsaHashedImportParams | EcKeyImportParams | AesKeyAlgorithm, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKey>;
  public unwrapKey(format: KeyFormat, wrappedKey: BufferSource, unwrappingKey: CryptoKey, unwrapAlgorithm: globalThis.AlgorithmIdentifier | RsaOaepParams | AesCtrParams | AesCbcParams | AesGcmParams, unwrappedKeyAlgorithm: globalThis.AlgorithmIdentifier | HmacImportParams | RsaHashedImportParams | EcKeyImportParams | AesKeyAlgorithm, extractable: boolean, keyUsages: Iterable<KeyUsage>): Promise<CryptoKey>;
  public async unwrapKey(...args: any[]): Promise<CryptoKey> {
    return (this.subtle.unwrapKey as any)(...args);
  }

  exportKey(format: "jwk", key: CryptoKey): Promise<JsonWebKey>;
  exportKey(format: "pkcs8" | "raw" | "spki", key: CryptoKey): Promise<ArrayBuffer>;
  exportKey(...args: any[]): Promise<ArrayBuffer> | Promise<JsonWebKey> {
    return (this.subtle.exportKey as any)(...args);
  }
  importKey(format: "jwk", keyData: JsonWebKey, algorithm: globalThis.AlgorithmIdentifier | RsaHashedImportParams | EcKeyImportParams | HmacImportParams | AesKeyAlgorithm, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKey>;
  importKey(format: "pkcs8" | "raw" | "spki", keyData: BufferSource, algorithm: globalThis.AlgorithmIdentifier | RsaHashedImportParams | EcKeyImportParams | HmacImportParams | AesKeyAlgorithm, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKey>;
  importKey(format: "jwk", keyData: JsonWebKey, algorithm: globalThis.AlgorithmIdentifier | RsaHashedImportParams | EcKeyImportParams | HmacImportParams | AesKeyAlgorithm, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKey>;
  importKey(format: "pkcs8" | "raw" | "spki", keyData: BufferSource, algorithm: globalThis.AlgorithmIdentifier | RsaHashedImportParams | EcKeyImportParams | HmacImportParams | AesKeyAlgorithm, extractable: boolean, keyUsages: Iterable<KeyUsage>): Promise<CryptoKey>;
  importKey(...args: any[]): Promise<CryptoKey> {
    return (this.subtle.importKey as any)(...args);
  }

  public getRandomValues<T extends ArrayBufferView | null>(array: T): T {
    return this.crypto.getRandomValues(array);
  }

}
