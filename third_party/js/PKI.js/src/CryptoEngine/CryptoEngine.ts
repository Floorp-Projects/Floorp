import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as pvtsutils from "pvtsutils";
import * as common from "../common";
import { PublicKeyInfo } from "../PublicKeyInfo";
import { PrivateKeyInfo } from "../PrivateKeyInfo";
import { AlgorithmIdentifier } from "../AlgorithmIdentifier";
import { EncryptedContentInfo } from "../EncryptedContentInfo";
import { IRSASSAPSSParams, RSASSAPSSParams } from "../RSASSAPSSParams";
import { PBKDF2Params } from "../PBKDF2Params";
import { PBES2Params } from "../PBES2Params";
import { ArgumentError, AsnError, ParameterError } from "../errors";
import * as type from "./CryptoEngineInterface";
import { AbstractCryptoEngine } from "./AbstractCryptoEngine";
import { EMPTY_STRING } from "../constants";
import { ECNamedCurves } from "../ECNamedCurves";

/**
 * Making MAC key using algorithm described in B.2 of PKCS#12 standard.
 */
async function makePKCS12B2Key(cryptoEngine: CryptoEngine, hashAlgorithm: string, keyLength: number, password: ArrayBuffer, salt: ArrayBuffer, iterationCount: number) {
  //#region Initial variables
  let u: number;
  let v: number;

  const result: number[] = [];
  //#endregion

  //#region Get "u" and "v" values
  switch (hashAlgorithm.toUpperCase()) {
    case "SHA-1":
      u = 20; // 160
      v = 64; // 512
      break;
    case "SHA-256":
      u = 32; // 256
      v = 64; // 512
      break;
    case "SHA-384":
      u = 48; // 384
      v = 128; // 1024
      break;
    case "SHA-512":
      u = 64; // 512
      v = 128; // 1024
      break;
    default:
      throw new Error("Unsupported hashing algorithm");
  }
  //#endregion

  //#region Main algorithm making key
  //#region Transform password to UTF-8 like string
  const passwordViewInitial = new Uint8Array(password);

  const passwordTransformed = new ArrayBuffer((password.byteLength * 2) + 2);
  const passwordTransformedView = new Uint8Array(passwordTransformed);

  for (let i = 0; i < passwordViewInitial.length; i++) {
    passwordTransformedView[i * 2] = 0x00;
    passwordTransformedView[i * 2 + 1] = passwordViewInitial[i];
  }

  passwordTransformedView[passwordTransformedView.length - 2] = 0x00;
  passwordTransformedView[passwordTransformedView.length - 1] = 0x00;

  password = passwordTransformed.slice(0);
  //#endregion

  //#region Construct a string D (the "diversifier") by concatenating v/8 copies of ID
  const D = new ArrayBuffer(v);
  const dView = new Uint8Array(D);

  for (let i = 0; i < D.byteLength; i++)
    dView[i] = 3; // The ID value equal to "3" for MACing (see B.3 of standard)
  //#endregion

  //#region Concatenate copies of the salt together to create a string S of length v * ceil(s / v) bytes (the final copy of the salt may be trunacted to create S)
  const saltLength = salt.byteLength;

  const sLen = v * Math.ceil(saltLength / v);
  const S = new ArrayBuffer(sLen);
  const sView = new Uint8Array(S);

  const saltView = new Uint8Array(salt);

  for (let i = 0; i < sLen; i++)
    sView[i] = saltView[i % saltLength];
  //#endregion

  //#region Concatenate copies of the password together to create a string P of length v * ceil(p / v) bytes (the final copy of the password may be truncated to create P)
  const passwordLength = password.byteLength;

  const pLen = v * Math.ceil(passwordLength / v);
  const P = new ArrayBuffer(pLen);
  const pView = new Uint8Array(P);

  const passwordView = new Uint8Array(password);

  for (let i = 0; i < pLen; i++)
    pView[i] = passwordView[i % passwordLength];
  //#endregion

  //#region Set I=S||P to be the concatenation of S and P
  const sPlusPLength = S.byteLength + P.byteLength;

  let I = new ArrayBuffer(sPlusPLength);
  let iView = new Uint8Array(I);

  iView.set(sView);
  iView.set(pView, sView.length);
  //#endregion

  //#region Set c=ceil(n / u)
  const c = Math.ceil((keyLength >> 3) / u);
  //#endregion

  //#region Initial variables
  let internalSequence = Promise.resolve(I);
  //#endregion

  //#region For i=1, 2, ..., c, do the following:
  for (let i = 0; i <= c; i++) {
    internalSequence = internalSequence.then(_I => {
      //#region Create contecanetion of D and I
      const dAndI = new ArrayBuffer(D.byteLength + _I.byteLength);
      const dAndIView = new Uint8Array(dAndI);

      dAndIView.set(dView);
      dAndIView.set(iView, dView.length);
      //#endregion

      return dAndI;
    });

    //#region Make "iterationCount" rounds of hashing
    for (let j = 0; j < iterationCount; j++)
      internalSequence = internalSequence.then(roundBuffer => cryptoEngine.digest({ name: hashAlgorithm }, new Uint8Array(roundBuffer)));
    //#endregion

    internalSequence = internalSequence.then(roundBuffer => {
      //#region Concatenate copies of Ai to create a string B of length v bits (the final copy of Ai may be truncated to create B)
      const B = new ArrayBuffer(v);
      const bView = new Uint8Array(B);

      for (let j = 0; j < B.byteLength; j++)
        bView[j] = (roundBuffer as any)[j % roundBuffer.byteLength]; // TODO roundBuffer is ArrayBuffer. It doesn't have indexed values
      //#endregion

      //#region Make new I value
      const k = Math.ceil(saltLength / v) + Math.ceil(passwordLength / v);
      const iRound = [];

      let sliceStart = 0;
      let sliceLength = v;

      for (let j = 0; j < k; j++) {
        const chunk = Array.from(new Uint8Array(I.slice(sliceStart, sliceStart + sliceLength)));
        sliceStart += v;
        if ((sliceStart + v) > I.byteLength)
          sliceLength = I.byteLength - sliceStart;

        let x = 0x1ff;

        for (let l = (B.byteLength - 1); l >= 0; l--) {
          x >>= 8;
          x += bView[l] + chunk[l];
          chunk[l] = (x & 0xff);
        }

        iRound.push(...chunk);
      }

      I = new ArrayBuffer(iRound.length);
      iView = new Uint8Array(I);

      iView.set(iRound);
      //#endregion

      result.push(...(new Uint8Array(roundBuffer)));

      return I;
    });
  }
  //#endregion

  //#region Initialize final key
  internalSequence = internalSequence.then(() => {
    const resultBuffer = new ArrayBuffer(keyLength >> 3);
    const resultView = new Uint8Array(resultBuffer);

    resultView.set((new Uint8Array(result)).slice(0, keyLength >> 3));

    return resultBuffer;
  });
  //#endregion
  //#endregion

  return internalSequence;
}

function prepareAlgorithm(data: globalThis.AlgorithmIdentifier | EcdsaParams): Algorithm & { hash?: Algorithm; } {
  const res = typeof data === "string"
    ? { name: data }
    : data;

  // TODO fix type casting `as EcdsaParams`
  if ("hash" in (res as EcdsaParams)) {
    return {
      ...res,
      hash: prepareAlgorithm((res as EcdsaParams).hash)
    };
  }

  return res;
}

/**
 * Default cryptographic engine for Web Cryptography API
 */
export class CryptoEngine extends AbstractCryptoEngine {

  public override async importKey(format: KeyFormat, keyData: BufferSource | JsonWebKey, algorithm: globalThis.AlgorithmIdentifier, extractable: boolean, keyUsages: KeyUsage[]): Promise<CryptoKey> {
    //#region Initial variables
    let jwk: JsonWebKey = {};
    //#endregion

    const alg = prepareAlgorithm(algorithm);

    switch (format.toLowerCase()) {
      case "raw":
        return this.subtle.importKey("raw", keyData as BufferSource, algorithm, extractable, keyUsages);
      case "spki":
        {
          const asn1 = asn1js.fromBER(pvtsutils.BufferSourceConverter.toArrayBuffer(keyData as BufferSource));
          AsnError.assert(asn1, "keyData");

          const publicKeyInfo = new PublicKeyInfo();
          try {
            publicKeyInfo.fromSchema(asn1.result);
          } catch {
            throw new ArgumentError("Incorrect keyData");
          }

          switch (alg.name.toUpperCase()) {
            case "RSA-PSS":
              {
                //#region Get information about used hash function
                if (!alg.hash) {
                  throw new ParameterError("hash", "algorithm.hash", "Incorrect hash algorithm: Hash algorithm is missed");
                }
                switch (alg.hash.name.toUpperCase()) {
                  case "SHA-1":
                    jwk.alg = "PS1";
                    break;
                  case "SHA-256":
                    jwk.alg = "PS256";
                    break;
                  case "SHA-384":
                    jwk.alg = "PS384";
                    break;
                  case "SHA-512":
                    jwk.alg = "PS512";
                    break;
                  default:
                    throw new Error(`Incorrect hash algorithm: ${alg.hash.name.toUpperCase()}`);
                }
                //#endregion
              }
            // break omitted
            // eslint-disable-next-line no-fallthrough
            case "RSASSA-PKCS1-V1_5":
              {
                keyUsages = ["verify"]; // Override existing keyUsages value since the key is a public key

                jwk.kty = "RSA";
                jwk.ext = extractable;
                jwk.key_ops = keyUsages;

                if (publicKeyInfo.algorithm.algorithmId !== "1.2.840.113549.1.1.1")
                  throw new Error(`Incorrect public key algorithm: ${publicKeyInfo.algorithm.algorithmId}`);

                //#region Get information about used hash function
                if (!jwk.alg) {
                  if (!alg.hash) {
                    throw new ParameterError("hash", "algorithm.hash", "Incorrect hash algorithm: Hash algorithm is missed");
                  }
                  switch (alg.hash.name.toUpperCase()) {
                    case "SHA-1":
                      jwk.alg = "RS1";
                      break;
                    case "SHA-256":
                      jwk.alg = "RS256";
                      break;
                    case "SHA-384":
                      jwk.alg = "RS384";
                      break;
                    case "SHA-512":
                      jwk.alg = "RS512";
                      break;
                    default:
                      throw new Error(`Incorrect hash algorithm: ${alg.hash.name.toUpperCase()}`);
                  }
                }
                //#endregion

                //#region Create RSA Public Key elements
                const publicKeyJSON = publicKeyInfo.toJSON();
                Object.assign(jwk, publicKeyJSON);
                //#endregion
              }
              break;
            case "ECDSA":
              keyUsages = ["verify"]; // Override existing keyUsages value since the key is a public key
            // break omitted
            // eslint-disable-next-line no-fallthrough
            case "ECDH":
              {
                //#region Initial variables
                jwk = {
                  kty: "EC",
                  ext: extractable,
                  key_ops: keyUsages
                };
                //#endregion

                //#region Get information about algorithm
                if (publicKeyInfo.algorithm.algorithmId !== "1.2.840.10045.2.1") {
                  throw new Error(`Incorrect public key algorithm: ${publicKeyInfo.algorithm.algorithmId}`);
                }
                //#endregion

                //#region Create ECDSA Public Key elements
                const publicKeyJSON = publicKeyInfo.toJSON();
                Object.assign(jwk, publicKeyJSON);
                //#endregion
              }
              break;
            case "RSA-OAEP":
              {
                jwk.kty = "RSA";
                jwk.ext = extractable;
                jwk.key_ops = keyUsages;

                if (this.name.toLowerCase() === "safari")
                  jwk.alg = "RSA-OAEP";
                else {
                  if (!alg.hash) {
                    throw new ParameterError("hash", "algorithm.hash", "Incorrect hash algorithm: Hash algorithm is missed");
                  }
                  switch (alg.hash.name.toUpperCase()) {
                    case "SHA-1":
                      jwk.alg = "RSA-OAEP";
                      break;
                    case "SHA-256":
                      jwk.alg = "RSA-OAEP-256";
                      break;
                    case "SHA-384":
                      jwk.alg = "RSA-OAEP-384";
                      break;
                    case "SHA-512":
                      jwk.alg = "RSA-OAEP-512";
                      break;
                    default:
                      throw new Error(`Incorrect hash algorithm: ${alg.hash.name.toUpperCase()}`);
                  }
                }

                //#region Create ECDSA Public Key elements
                const publicKeyJSON = publicKeyInfo.toJSON();
                Object.assign(jwk, publicKeyJSON);
                //#endregion
              }
              break;
            case "RSAES-PKCS1-V1_5":
              {
                jwk.kty = "RSA";
                jwk.ext = extractable;
                jwk.key_ops = keyUsages;
                jwk.alg = "PS1";

                const publicKeyJSON = publicKeyInfo.toJSON();
                Object.assign(jwk, publicKeyJSON);
              }
              break;
            default:
              throw new Error(`Incorrect algorithm name: ${alg.name.toUpperCase()}`);
          }
        }
        break;
      case "pkcs8":
        {
          const privateKeyInfo = new PrivateKeyInfo();

          //#region Parse "PrivateKeyInfo" object
          const asn1 = asn1js.fromBER(pvtsutils.BufferSourceConverter.toArrayBuffer(keyData as BufferSource));
          AsnError.assert(asn1, "keyData");

          try {
            privateKeyInfo.fromSchema(asn1.result);
          }
          catch (ex) {
            throw new Error("Incorrect keyData");
          }

          if (!privateKeyInfo.parsedKey)
            throw new Error("Incorrect keyData");
          //#endregion

          switch (alg.name.toUpperCase()) {
            case "RSA-PSS":
              {
                //#region Get information about used hash function
                switch (alg.hash?.name.toUpperCase()) {
                  case "SHA-1":
                    jwk.alg = "PS1";
                    break;
                  case "SHA-256":
                    jwk.alg = "PS256";
                    break;
                  case "SHA-384":
                    jwk.alg = "PS384";
                    break;
                  case "SHA-512":
                    jwk.alg = "PS512";
                    break;
                  default:
                    throw new Error(`Incorrect hash algorithm: ${alg.hash?.name.toUpperCase()}`);
                }
                //#endregion
              }
            // break omitted
            // eslint-disable-next-line no-fallthrough
            case "RSASSA-PKCS1-V1_5":
              {
                keyUsages = ["sign"]; // Override existing keyUsages value since the key is a private key

                jwk.kty = "RSA";
                jwk.ext = extractable;
                jwk.key_ops = keyUsages;

                //#region Get information about used hash function
                if (privateKeyInfo.privateKeyAlgorithm.algorithmId !== "1.2.840.113549.1.1.1")
                  throw new Error(`Incorrect private key algorithm: ${privateKeyInfo.privateKeyAlgorithm.algorithmId}`);
                //#endregion

                //#region Get information about used hash function
                if (("alg" in jwk) === false) {
                  switch (alg.hash?.name.toUpperCase()) {
                    case "SHA-1":
                      jwk.alg = "RS1";
                      break;
                    case "SHA-256":
                      jwk.alg = "RS256";
                      break;
                    case "SHA-384":
                      jwk.alg = "RS384";
                      break;
                    case "SHA-512":
                      jwk.alg = "RS512";
                      break;
                    default:
                      throw new Error(`Incorrect hash algorithm: ${alg.hash?.name.toUpperCase()}`);
                  }
                }
                //#endregion

                //#region Create RSA Private Key elements
                const privateKeyJSON = privateKeyInfo.toJSON();
                Object.assign(jwk, privateKeyJSON);
                //#endregion
              }
              break;
            case "ECDSA":
              keyUsages = ["sign"]; // Override existing keyUsages value since the key is a private key
            // break omitted
            // eslint-disable-next-line no-fallthrough
            case "ECDH":
              {
                //#region Initial variables
                jwk = {
                  kty: "EC",
                  ext: extractable,
                  key_ops: keyUsages
                };
                //#endregion

                //#region Get information about used hash function
                if (privateKeyInfo.privateKeyAlgorithm.algorithmId !== "1.2.840.10045.2.1")
                  throw new Error(`Incorrect algorithm: ${privateKeyInfo.privateKeyAlgorithm.algorithmId}`);
                //#endregion

                //#region Create ECDSA Private Key elements
                const privateKeyJSON = privateKeyInfo.toJSON();
                Object.assign(jwk, privateKeyJSON);
                //#endregion
              }
              break;
            case "RSA-OAEP":
              {
                jwk.kty = "RSA";
                jwk.ext = extractable;
                jwk.key_ops = keyUsages;

                //#region Get information about used hash function
                if (this.name.toLowerCase() === "safari")
                  jwk.alg = "RSA-OAEP";
                else {
                  switch (alg.hash?.name.toUpperCase()) {
                    case "SHA-1":
                      jwk.alg = "RSA-OAEP";
                      break;
                    case "SHA-256":
                      jwk.alg = "RSA-OAEP-256";
                      break;
                    case "SHA-384":
                      jwk.alg = "RSA-OAEP-384";
                      break;
                    case "SHA-512":
                      jwk.alg = "RSA-OAEP-512";
                      break;
                    default:
                      throw new Error(`Incorrect hash algorithm: ${alg.hash?.name.toUpperCase()}`);
                  }
                }
                //#endregion

                //#region Create RSA Private Key elements
                const privateKeyJSON = privateKeyInfo.toJSON();
                Object.assign(jwk, privateKeyJSON);
                //#endregion
              }
              break;
            case "RSAES-PKCS1-V1_5":
              {
                keyUsages = ["decrypt"]; // Override existing keyUsages value since the key is a private key

                jwk.kty = "RSA";
                jwk.ext = extractable;
                jwk.key_ops = keyUsages;
                jwk.alg = "PS1";

                //#region Create RSA Private Key elements
                const privateKeyJSON = privateKeyInfo.toJSON();
                Object.assign(jwk, privateKeyJSON);
                //#endregion
              }
              break;
            default:
              throw new Error(`Incorrect algorithm name: ${alg.name.toUpperCase()}`);
          }
        }
        break;
      case "jwk":
        jwk = keyData as JsonWebKey;
        break;
      default:
        throw new Error(`Incorrect format: ${format}`);
    }

    //#region Special case for Safari browser (since its acting not as WebCrypto standard describes)
    if (this.name.toLowerCase() === "safari") {
      // Try to use both ways - import using ArrayBuffer and pure JWK (for Safari Technology Preview)
      try {
        return this.subtle.importKey("jwk", pvutils.stringToArrayBuffer(JSON.stringify(jwk)) as any, algorithm, extractable, keyUsages);
      } catch {
        return this.subtle.importKey("jwk", jwk, algorithm, extractable, keyUsages);
      }
    }
    //#endregion

    return this.subtle.importKey("jwk", jwk, algorithm, extractable, keyUsages);
  }

  /**
   * Export WebCrypto keys to different formats
   * @param format
   * @param key
   */
  public override exportKey(format: "jwk", key: CryptoKey): Promise<JsonWebKey>;
  public override exportKey(format: Exclude<KeyFormat, "jwk">, key: CryptoKey): Promise<ArrayBuffer>;
  public override exportKey(format: string, key: CryptoKey): Promise<ArrayBuffer | JsonWebKey>;
  public override async exportKey(format: KeyFormat, key: CryptoKey): Promise<ArrayBuffer | JsonWebKey> {
    let jwk = await this.subtle.exportKey("jwk", key);

    //#region Currently Safari returns ArrayBuffer as JWK thus we need an additional transformation
    if (this.name.toLowerCase() === "safari") {
      // Some additional checks for Safari Technology Preview
      if (jwk instanceof ArrayBuffer) {
        jwk = JSON.parse(pvutils.arrayBufferToString(jwk));
      }
    }
    //#endregion

    switch (format.toLowerCase()) {
      case "raw":
        return this.subtle.exportKey("raw", key);
      case "spki": {
        const publicKeyInfo = new PublicKeyInfo();

        try {
          publicKeyInfo.fromJSON(jwk);
        }
        catch (ex) {
          throw new Error("Incorrect key data");
        }

        return publicKeyInfo.toSchema().toBER(false);
      }
      case "pkcs8": {
        const privateKeyInfo = new PrivateKeyInfo();

        try {
          privateKeyInfo.fromJSON(jwk);
        }
        catch (ex) {
          throw new Error("Incorrect key data");
        }

        return privateKeyInfo.toSchema().toBER(false);
      }
      case "jwk":
        return jwk;
      default:
        throw new Error(`Incorrect format: ${format}`);
    }
  }

  /**
   * Convert WebCrypto keys between different export formats
   * @param  inputFormat
   * @param  outputFormat
   * @param  keyData
   * @param  algorithm
   * @param  extractable
   * @param  keyUsages
   */
  public async convert(inputFormat: KeyFormat, outputFormat: KeyFormat, keyData: ArrayBuffer | JsonWebKey, algorithm: Algorithm, extractable: boolean, keyUsages: KeyUsage[]) {
    if (inputFormat.toLowerCase() === outputFormat.toLowerCase()) {
      return keyData;
    }

    const key = await this.importKey(inputFormat, keyData, algorithm, extractable, keyUsages);
    return this.exportKey(outputFormat, key);
  }

  /**
   * Gets WebCrypto algorithm by wel-known OID
   * @param oid algorithm identifier
   * @param safety if `true` throws exception on unknown algorithm identifier
   * @param target name of the target
   * @returns Returns WebCrypto algorithm or an empty object
   */
  public getAlgorithmByOID<T extends Algorithm = Algorithm>(oid: string, safety?: boolean, target?: string): T | object;
  /**
   * Gets WebCrypto algorithm by wel-known OID
   * @param oid algorithm identifier
   * @param safety if `true` throws exception on unknown algorithm identifier
   * @param target name of the target
   * @returns Returns WebCrypto algorithm
   * @throws Throws {@link Error} exception if unknown algorithm identifier
   */
  public getAlgorithmByOID<T extends Algorithm = Algorithm>(oid: string, safety: true, target?: string): T;
  public getAlgorithmByOID(oid: string, safety = false, target?: string): any {
    switch (oid) {
      case "1.2.840.113549.1.1.1":
        return {
          name: "RSAES-PKCS1-v1_5"
        };
      case "1.2.840.113549.1.1.5":
        return {
          name: "RSASSA-PKCS1-v1_5",
          hash: {
            name: "SHA-1"
          }
        };
      case "1.2.840.113549.1.1.11":
        return {
          name: "RSASSA-PKCS1-v1_5",
          hash: {
            name: "SHA-256"
          }
        };
      case "1.2.840.113549.1.1.12":
        return {
          name: "RSASSA-PKCS1-v1_5",
          hash: {
            name: "SHA-384"
          }
        };
      case "1.2.840.113549.1.1.13":
        return {
          name: "RSASSA-PKCS1-v1_5",
          hash: {
            name: "SHA-512"
          }
        };
      case "1.2.840.113549.1.1.10":
        return {
          name: "RSA-PSS"
        };
      case "1.2.840.113549.1.1.7":
        return {
          name: "RSA-OAEP"
        };
      case "1.2.840.10045.2.1":
      case "1.2.840.10045.4.1":
        return {
          name: "ECDSA",
          hash: {
            name: "SHA-1"
          }
        };
      case "1.2.840.10045.4.3.2":
        return {
          name: "ECDSA",
          hash: {
            name: "SHA-256"
          }
        };
      case "1.2.840.10045.4.3.3":
        return {
          name: "ECDSA",
          hash: {
            name: "SHA-384"
          }
        };
      case "1.2.840.10045.4.3.4":
        return {
          name: "ECDSA",
          hash: {
            name: "SHA-512"
          }
        };
      case "1.3.133.16.840.63.0.2":
        return {
          name: "ECDH",
          kdf: "SHA-1"
        };
      case "1.3.132.1.11.1":
        return {
          name: "ECDH",
          kdf: "SHA-256"
        };
      case "1.3.132.1.11.2":
        return {
          name: "ECDH",
          kdf: "SHA-384"
        };
      case "1.3.132.1.11.3":
        return {
          name: "ECDH",
          kdf: "SHA-512"
        };
      case "2.16.840.1.101.3.4.1.2":
        return {
          name: "AES-CBC",
          length: 128
        };
      case "2.16.840.1.101.3.4.1.22":
        return {
          name: "AES-CBC",
          length: 192
        };
      case "2.16.840.1.101.3.4.1.42":
        return {
          name: "AES-CBC",
          length: 256
        };
      case "2.16.840.1.101.3.4.1.6":
        return {
          name: "AES-GCM",
          length: 128
        };
      case "2.16.840.1.101.3.4.1.26":
        return {
          name: "AES-GCM",
          length: 192
        };
      case "2.16.840.1.101.3.4.1.46":
        return {
          name: "AES-GCM",
          length: 256
        };
      case "2.16.840.1.101.3.4.1.4":
        return {
          name: "AES-CFB",
          length: 128
        };
      case "2.16.840.1.101.3.4.1.24":
        return {
          name: "AES-CFB",
          length: 192
        };
      case "2.16.840.1.101.3.4.1.44":
        return {
          name: "AES-CFB",
          length: 256
        };
      case "2.16.840.1.101.3.4.1.5":
        return {
          name: "AES-KW",
          length: 128
        };
      case "2.16.840.1.101.3.4.1.25":
        return {
          name: "AES-KW",
          length: 192
        };
      case "2.16.840.1.101.3.4.1.45":
        return {
          name: "AES-KW",
          length: 256
        };
      case "1.2.840.113549.2.7":
        return {
          name: "HMAC",
          hash: {
            name: "SHA-1"
          }
        };
      case "1.2.840.113549.2.9":
        return {
          name: "HMAC",
          hash: {
            name: "SHA-256"
          }
        };
      case "1.2.840.113549.2.10":
        return {
          name: "HMAC",
          hash: {
            name: "SHA-384"
          }
        };
      case "1.2.840.113549.2.11":
        return {
          name: "HMAC",
          hash: {
            name: "SHA-512"
          }
        };
      case "1.2.840.113549.1.9.16.3.5":
        return {
          name: "DH"
        };
      case "1.3.14.3.2.26":
        return {
          name: "SHA-1"
        };
      case "2.16.840.1.101.3.4.2.1":
        return {
          name: "SHA-256"
        };
      case "2.16.840.1.101.3.4.2.2":
        return {
          name: "SHA-384"
        };
      case "2.16.840.1.101.3.4.2.3":
        return {
          name: "SHA-512"
        };
      case "1.2.840.113549.1.5.12":
        return {
          name: "PBKDF2"
        };
      //#region Special case - OIDs for ECC curves
      case "1.2.840.10045.3.1.7":
        return {
          name: "P-256"
        };
      case "1.3.132.0.34":
        return {
          name: "P-384"
        };
      case "1.3.132.0.35":
        return {
          name: "P-521"
        };
      //#endregion
      default:
    }

    if (safety) {
      throw new Error(`Unsupported algorithm identifier ${target ? `for ${target} ` : EMPTY_STRING}: ${oid}`);
    }

    return {};
  }

  public getOIDByAlgorithm(algorithm: Algorithm, safety = false, target?: string): string {
    let result = EMPTY_STRING;

    switch (algorithm.name.toUpperCase()) {
      case "RSAES-PKCS1-V1_5":
        result = "1.2.840.113549.1.1.1";
        break;
      case "RSASSA-PKCS1-V1_5":
        switch ((algorithm as any).hash.name.toUpperCase()) {
          case "SHA-1":
            result = "1.2.840.113549.1.1.5";
            break;
          case "SHA-256":
            result = "1.2.840.113549.1.1.11";
            break;
          case "SHA-384":
            result = "1.2.840.113549.1.1.12";
            break;
          case "SHA-512":
            result = "1.2.840.113549.1.1.13";
            break;
          default:
        }
        break;
      case "RSA-PSS":
        result = "1.2.840.113549.1.1.10";
        break;
      case "RSA-OAEP":
        result = "1.2.840.113549.1.1.7";
        break;
      case "ECDSA":
        switch ((algorithm as any).hash.name.toUpperCase()) {
          case "SHA-1":
            result = "1.2.840.10045.4.1";
            break;
          case "SHA-256":
            result = "1.2.840.10045.4.3.2";
            break;
          case "SHA-384":
            result = "1.2.840.10045.4.3.3";
            break;
          case "SHA-512":
            result = "1.2.840.10045.4.3.4";
            break;
          default:
        }
        break;
      case "ECDH":
        switch ((algorithm as any).kdf.toUpperCase()) // Non-standard addition - hash algorithm of KDF function
        {
          case "SHA-1":
            result = "1.3.133.16.840.63.0.2"; // dhSinglePass-stdDH-sha1kdf-scheme
            break;
          case "SHA-256":
            result = "1.3.132.1.11.1"; // dhSinglePass-stdDH-sha256kdf-scheme
            break;
          case "SHA-384":
            result = "1.3.132.1.11.2"; // dhSinglePass-stdDH-sha384kdf-scheme
            break;
          case "SHA-512":
            result = "1.3.132.1.11.3"; // dhSinglePass-stdDH-sha512kdf-scheme
            break;
          default:
        }
        break;
      case "AES-CTR":
        break;
      case "AES-CBC":
        switch ((algorithm as any).length) {
          case 128:
            result = "2.16.840.1.101.3.4.1.2";
            break;
          case 192:
            result = "2.16.840.1.101.3.4.1.22";
            break;
          case 256:
            result = "2.16.840.1.101.3.4.1.42";
            break;
          default:
        }
        break;
      case "AES-CMAC":
        break;
      case "AES-GCM":
        switch ((algorithm as any).length) {
          case 128:
            result = "2.16.840.1.101.3.4.1.6";
            break;
          case 192:
            result = "2.16.840.1.101.3.4.1.26";
            break;
          case 256:
            result = "2.16.840.1.101.3.4.1.46";
            break;
          default:
        }
        break;
      case "AES-CFB":
        switch ((algorithm as any).length) {
          case 128:
            result = "2.16.840.1.101.3.4.1.4";
            break;
          case 192:
            result = "2.16.840.1.101.3.4.1.24";
            break;
          case 256:
            result = "2.16.840.1.101.3.4.1.44";
            break;
          default:
        }
        break;
      case "AES-KW":
        switch ((algorithm as any).length) {
          case 128:
            result = "2.16.840.1.101.3.4.1.5";
            break;
          case 192:
            result = "2.16.840.1.101.3.4.1.25";
            break;
          case 256:
            result = "2.16.840.1.101.3.4.1.45";
            break;
          default:
        }
        break;
      case "HMAC":
        switch ((algorithm as any).hash.name.toUpperCase()) {
          case "SHA-1":
            result = "1.2.840.113549.2.7";
            break;
          case "SHA-256":
            result = "1.2.840.113549.2.9";
            break;
          case "SHA-384":
            result = "1.2.840.113549.2.10";
            break;
          case "SHA-512":
            result = "1.2.840.113549.2.11";
            break;
          default:
        }
        break;
      case "DH":
        result = "1.2.840.113549.1.9.16.3.5";
        break;
      case "SHA-1":
        result = "1.3.14.3.2.26";
        break;
      case "SHA-256":
        result = "2.16.840.1.101.3.4.2.1";
        break;
      case "SHA-384":
        result = "2.16.840.1.101.3.4.2.2";
        break;
      case "SHA-512":
        result = "2.16.840.1.101.3.4.2.3";
        break;
      case "CONCAT":
        break;
      case "HKDF":
        break;
      case "PBKDF2":
        result = "1.2.840.113549.1.5.12";
        break;
      //#region Special case - OIDs for ECC curves
      case "P-256":
        result = "1.2.840.10045.3.1.7";
        break;
      case "P-384":
        result = "1.3.132.0.34";
        break;
      case "P-521":
        result = "1.3.132.0.35";
        break;
      //#endregion
      default:
    }

    if (!result && safety) {
      throw new Error(`Unsupported algorithm ${target ? `for ${target} ` : EMPTY_STRING}: ${algorithm.name}`);
    }

    return result;
  }

  public getAlgorithmParameters(algorithmName: string, operation: type.CryptoEngineAlgorithmOperation): type.CryptoEngineAlgorithmParams {
    let result: type.CryptoEngineAlgorithmParams = {
      algorithm: {},
      usages: []
    };

    switch (algorithmName.toUpperCase()) {
      case "RSAES-PKCS1-V1_5":
      case "RSASSA-PKCS1-V1_5":
        switch (operation.toLowerCase()) {
          case "generatekey":
            result = {
              algorithm: {
                name: "RSASSA-PKCS1-v1_5",
                modulusLength: 2048,
                publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
                hash: {
                  name: "SHA-256"
                }
              },
              usages: ["sign", "verify"]
            };
            break;
          case "verify":
          case "sign":
          case "importkey":
            result = {
              algorithm: {
                name: "RSASSA-PKCS1-v1_5",
                hash: {
                  name: "SHA-256"
                }
              },
              usages: ["verify"] // For importKey("pkcs8") usage must be "sign" only
            };
            break;
          case "exportkey":
          default:
            return {
              algorithm: {
                name: "RSASSA-PKCS1-v1_5"
              },
              usages: []
            };
        }
        break;
      case "RSA-PSS":
        switch (operation.toLowerCase()) {
          case "sign":
          case "verify":
            result = {
              algorithm: {
                name: "RSA-PSS",
                hash: {
                  name: "SHA-1"
                },
                saltLength: 20
              },
              usages: ["sign", "verify"]
            };
            break;
          case "generatekey":
            result = {
              algorithm: {
                name: "RSA-PSS",
                modulusLength: 2048,
                publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
                hash: {
                  name: "SHA-1"
                }
              },
              usages: ["sign", "verify"]
            };
            break;
          case "importkey":
            result = {
              algorithm: {
                name: "RSA-PSS",
                hash: {
                  name: "SHA-1"
                }
              },
              usages: ["verify"] // For importKey("pkcs8") usage must be "sign" only
            };
            break;
          case "exportkey":
          default:
            return {
              algorithm: {
                name: "RSA-PSS"
              },
              usages: []
            };
        }
        break;
      case "RSA-OAEP":
        switch (operation.toLowerCase()) {
          case "encrypt":
          case "decrypt":
            result = {
              algorithm: {
                name: "RSA-OAEP"
              },
              usages: ["encrypt", "decrypt"]
            };
            break;
          case "generatekey":
            result = {
              algorithm: {
                name: "RSA-OAEP",
                modulusLength: 2048,
                publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
                hash: {
                  name: "SHA-256"
                }
              },
              usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"]
            };
            break;
          case "importkey":
            result = {
              algorithm: {
                name: "RSA-OAEP",
                hash: {
                  name: "SHA-256"
                }
              },
              usages: ["encrypt"] // encrypt for "spki" and decrypt for "pkcs8"
            };
            break;
          case "exportkey":
          default:
            return {
              algorithm: {
                name: "RSA-OAEP"
              },
              usages: []
            };
        }
        break;
      case "ECDSA":
        switch (operation.toLowerCase()) {
          case "generatekey":
            result = {
              algorithm: {
                name: "ECDSA",
                namedCurve: "P-256"
              },
              usages: ["sign", "verify"]
            };
            break;
          case "importkey":
            result = {
              algorithm: {
                name: "ECDSA",
                namedCurve: "P-256"
              },
              usages: ["verify"] // "sign" for "pkcs8"
            };
            break;
          case "verify":
          case "sign":
            result = {
              algorithm: {
                name: "ECDSA",
                hash: {
                  name: "SHA-256"
                }
              },
              usages: ["sign"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "ECDSA"
              },
              usages: []
            };
        }
        break;
      case "ECDH":
        switch (operation.toLowerCase()) {
          case "exportkey":
          case "importkey":
          case "generatekey":
            result = {
              algorithm: {
                name: "ECDH",
                namedCurve: "P-256"
              },
              usages: ["deriveKey", "deriveBits"]
            };
            break;
          case "derivekey":
          case "derivebits":
            result = {
              algorithm: {
                name: "ECDH",
                namedCurve: "P-256",
                public: [] // Must be a "publicKey"
              },
              usages: ["encrypt", "decrypt"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "ECDH"
              },
              usages: []
            };
        }
        break;
      case "AES-CTR":
        switch (operation.toLowerCase()) {
          case "importkey":
          case "exportkey":
          case "generatekey":
            result = {
              algorithm: {
                name: "AES-CTR",
                length: 256
              },
              usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"]
            };
            break;
          case "decrypt":
          case "encrypt":
            result = {
              algorithm: {
                name: "AES-CTR",
                counter: new Uint8Array(16),
                length: 10
              },
              usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "AES-CTR"
              },
              usages: []
            };
        }
        break;
      case "AES-CBC":
        switch (operation.toLowerCase()) {
          case "importkey":
          case "exportkey":
          case "generatekey":
            result = {
              algorithm: {
                name: "AES-CBC",
                length: 256
              },
              usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"]
            };
            break;
          case "decrypt":
          case "encrypt":
            result = {
              algorithm: {
                name: "AES-CBC",
                iv: this.getRandomValues(new Uint8Array(16)) // For "decrypt" the value should be replaced with value got on "encrypt" step
              },
              usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "AES-CBC"
              },
              usages: []
            };
        }
        break;
      case "AES-GCM":
        switch (operation.toLowerCase()) {
          case "importkey":
          case "exportkey":
          case "generatekey":
            result = {
              algorithm: {
                name: "AES-GCM",
                length: 256
              },
              usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"]
            };
            break;
          case "decrypt":
          case "encrypt":
            result = {
              algorithm: {
                name: "AES-GCM",
                iv: this.getRandomValues(new Uint8Array(16)) // For "decrypt" the value should be replaced with value got on "encrypt" step
              },
              usages: ["encrypt", "decrypt", "wrapKey", "unwrapKey"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "AES-GCM"
              },
              usages: []
            };
        }
        break;
      case "AES-KW":
        switch (operation.toLowerCase()) {
          case "importkey":
          case "exportkey":
          case "generatekey":
          case "wrapkey":
          case "unwrapkey":
            result = {
              algorithm: {
                name: "AES-KW",
                length: 256
              },
              usages: ["wrapKey", "unwrapKey"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "AES-KW"
              },
              usages: []
            };
        }
        break;
      case "HMAC":
        switch (operation.toLowerCase()) {
          case "sign":
          case "verify":
            result = {
              algorithm: {
                name: "HMAC"
              },
              usages: ["sign", "verify"]
            };
            break;
          case "importkey":
          case "exportkey":
          case "generatekey":
            result = {
              algorithm: {
                name: "HMAC",
                length: 32,
                hash: {
                  name: "SHA-256"
                }
              },
              usages: ["sign", "verify"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "HMAC"
              },
              usages: []
            };
        }
        break;
      case "HKDF":
        switch (operation.toLowerCase()) {
          case "derivekey":
            result = {
              algorithm: {
                name: "HKDF",
                hash: "SHA-256",
                salt: new Uint8Array([]),
                info: new Uint8Array([])
              },
              usages: ["encrypt", "decrypt"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "HKDF"
              },
              usages: []
            };
        }
        break;
      case "PBKDF2":
        switch (operation.toLowerCase()) {
          case "derivekey":
            result = {
              algorithm: {
                name: "PBKDF2",
                hash: { name: "SHA-256" },
                salt: new Uint8Array([]),
                iterations: 10000
              },
              usages: ["encrypt", "decrypt"]
            };
            break;
          default:
            return {
              algorithm: {
                name: "PBKDF2"
              },
              usages: []
            };
        }
        break;
      default:
    }

    return result;
  }

  /**
   * Getting hash algorithm by signature algorithm
   * @param signatureAlgorithm Signature algorithm
   */
  // TODO use safety
  getHashAlgorithm(signatureAlgorithm: AlgorithmIdentifier): string {
    let result = EMPTY_STRING;

    switch (signatureAlgorithm.algorithmId) {
      case "1.2.840.10045.4.1": // ecdsa-with-SHA1
      case "1.2.840.113549.1.1.5": // rsa-encryption-with-SHA1
        result = "SHA-1";
        break;
      case "1.2.840.10045.4.3.2": // ecdsa-with-SHA256
      case "1.2.840.113549.1.1.11": // rsa-encryption-with-SHA256
        result = "SHA-256";
        break;
      case "1.2.840.10045.4.3.3": // ecdsa-with-SHA384
      case "1.2.840.113549.1.1.12": // rsa-encryption-with-SHA384
        result = "SHA-384";
        break;
      case "1.2.840.10045.4.3.4": // ecdsa-with-SHA512
      case "1.2.840.113549.1.1.13": // rsa-encryption-with-SHA512
        result = "SHA-512";
        break;
      case "1.2.840.113549.1.1.10": // RSA-PSS
        {
          try {
            const params = new RSASSAPSSParams({ schema: signatureAlgorithm.algorithmParams });
            if (params.hashAlgorithm) {
              const algorithm = this.getAlgorithmByOID(params.hashAlgorithm.algorithmId);
              if ("name" in algorithm) {
                result = algorithm.name;
              }
              else {
                return EMPTY_STRING;
              }
            }
            else
              result = "SHA-1";
          }
          catch {
            // nothing
          }
        }
        break;
      default:
    }

    return result;
  }

  public async encryptEncryptedContentInfo(parameters: type.CryptoEngineEncryptParams): Promise<EncryptedContentInfo> {
    //#region Check for input parameters
    ParameterError.assert(parameters,
      "password", "contentEncryptionAlgorithm", "hmacHashAlgorithm",
      "iterationCount", "contentToEncrypt", "contentToEncrypt", "contentType");

    const contentEncryptionOID = this.getOIDByAlgorithm(parameters.contentEncryptionAlgorithm, true, "contentEncryptionAlgorithm");

    const pbkdf2OID = this.getOIDByAlgorithm({
      name: "PBKDF2"
    }, true, "PBKDF2");
    const hmacOID = this.getOIDByAlgorithm({
      name: "HMAC",
      hash: {
        name: parameters.hmacHashAlgorithm
      }
    } as Algorithm, true, "hmacHashAlgorithm");
    //#endregion

    //#region Initial variables

    // TODO Should we reuse iv from parameters.contentEncryptionAlgorithm or use it's length for ivBuffer?
    const ivBuffer = new ArrayBuffer(16); // For AES we need IV 16 bytes long
    const ivView = new Uint8Array(ivBuffer);
    this.getRandomValues(ivView);

    const saltBuffer = new ArrayBuffer(64);
    const saltView = new Uint8Array(saltBuffer);
    this.getRandomValues(saltView);

    const contentView = new Uint8Array(parameters.contentToEncrypt);

    const pbkdf2Params = new PBKDF2Params({
      salt: new asn1js.OctetString({ valueHex: saltBuffer }),
      iterationCount: parameters.iterationCount,
      prf: new AlgorithmIdentifier({
        algorithmId: hmacOID,
        algorithmParams: new asn1js.Null()
      })
    });
    //#endregion

    //#region Derive PBKDF2 key from "password" buffer
    const passwordView = new Uint8Array(parameters.password);

    const pbkdfKey = await this.importKey("raw",
      passwordView,
      "PBKDF2",
      false,
      ["deriveKey"]);

    //#endregion

    //#region Derive key for "contentEncryptionAlgorithm"
    const derivedKey = await this.deriveKey({
      name: "PBKDF2",
      hash: {
        name: parameters.hmacHashAlgorithm
      },
      salt: saltView,
      iterations: parameters.iterationCount
    },
      pbkdfKey,
      parameters.contentEncryptionAlgorithm,
      false,
      ["encrypt"]);
    //#endregion

    //#region Encrypt content
    // TODO encrypt doesn't use all parameters from parameters.contentEncryptionAlgorithm (eg additionalData and tagLength for AES-GCM)
    const encryptedData = await this.encrypt(
      {
        name: parameters.contentEncryptionAlgorithm.name,
        iv: ivView
      },
      derivedKey,
      contentView);
    //#endregion

    //#region Store all parameters in EncryptedData object
    const pbes2Parameters = new PBES2Params({
      keyDerivationFunc: new AlgorithmIdentifier({
        algorithmId: pbkdf2OID,
        algorithmParams: pbkdf2Params.toSchema()
      }),
      encryptionScheme: new AlgorithmIdentifier({
        algorithmId: contentEncryptionOID,
        algorithmParams: new asn1js.OctetString({ valueHex: ivBuffer })
      })
    });

    return new EncryptedContentInfo({
      contentType: parameters.contentType,
      contentEncryptionAlgorithm: new AlgorithmIdentifier({
        algorithmId: "1.2.840.113549.1.5.13", // pkcs5PBES2
        algorithmParams: pbes2Parameters.toSchema()
      }),
      encryptedContent: new asn1js.OctetString({ valueHex: encryptedData })
    });
    //#endregion
  }

  /**
   * Decrypt data stored in "EncryptedContentInfo" object using parameters
   * @param parameters
   */
  public async decryptEncryptedContentInfo(parameters: type.CryptoEngineDecryptParams): Promise<ArrayBuffer> {
    //#region Check for input parameters
    ParameterError.assert(parameters, "password", "encryptedContentInfo");

    if (parameters.encryptedContentInfo.contentEncryptionAlgorithm.algorithmId !== "1.2.840.113549.1.5.13") // pkcs5PBES2
      throw new Error(`Unknown "contentEncryptionAlgorithm": ${parameters.encryptedContentInfo.contentEncryptionAlgorithm.algorithmId}`);
    //#endregion

    //#region Initial variables
    let pbes2Parameters: PBES2Params;

    try {
      pbes2Parameters = new PBES2Params({ schema: parameters.encryptedContentInfo.contentEncryptionAlgorithm.algorithmParams });
    }
    catch (ex) {
      throw new Error("Incorrectly encoded \"pbes2Parameters\"");
    }

    let pbkdf2Params;

    try {
      pbkdf2Params = new PBKDF2Params({ schema: pbes2Parameters.keyDerivationFunc.algorithmParams });
    }
    catch (ex) {
      throw new Error("Incorrectly encoded \"pbkdf2Params\"");
    }

    const contentEncryptionAlgorithm = this.getAlgorithmByOID(pbes2Parameters.encryptionScheme.algorithmId, true);

    const ivBuffer = pbes2Parameters.encryptionScheme.algorithmParams.valueBlock.valueHex;
    const ivView = new Uint8Array(ivBuffer);

    const saltBuffer = pbkdf2Params.salt.valueBlock.valueHex;
    const saltView = new Uint8Array(saltBuffer);

    const iterationCount = pbkdf2Params.iterationCount;

    let hmacHashAlgorithm = "SHA-1";

    if (pbkdf2Params.prf) {
      const algorithm = this.getAlgorithmByOID<any>(pbkdf2Params.prf.algorithmId, true);
      hmacHashAlgorithm = algorithm.hash.name;
    }
    //#endregion

    //#region Derive PBKDF2 key from "password" buffer
    const pbkdfKey = await this.importKey("raw",
      parameters.password,
      "PBKDF2",
      false,
      ["deriveKey"]);
    //#endregion

    //#region Derive key for "contentEncryptionAlgorithm"
    const result = await this.deriveKey(
      {
        name: "PBKDF2",
        hash: {
          name: hmacHashAlgorithm
        },
        salt: saltView,
        iterations: iterationCount
      },
      pbkdfKey,
      contentEncryptionAlgorithm as any,
      false,
      ["decrypt"]);
    //#endregion

    //#region Decrypt internal content using derived key
    //#region Create correct data block for decryption
    const dataBuffer = parameters.encryptedContentInfo.getEncryptedContent();
    //#endregion

    return this.decrypt({
      name: contentEncryptionAlgorithm.name,
      iv: ivView
    },
      result,
      dataBuffer);
    //#endregion
  }

  public async stampDataWithPassword(parameters: type.CryptoEngineStampDataWithPasswordParams): Promise<ArrayBuffer> {
    //#region Check for input parameters
    if ((parameters instanceof Object) === false)
      throw new Error("Parameters must have type \"Object\"");

    ParameterError.assert(parameters, "password", "hashAlgorithm", "iterationCount", "salt", "contentToStamp");
    //#endregion

    //#region Choose correct length for HMAC key
    let length: number;

    switch (parameters.hashAlgorithm.toLowerCase()) {
      case "sha-1":
        length = 160;
        break;
      case "sha-256":
        length = 256;
        break;
      case "sha-384":
        length = 384;
        break;
      case "sha-512":
        length = 512;
        break;
      default:
        throw new Error(`Incorrect "parameters.hashAlgorithm" parameter: ${parameters.hashAlgorithm}`);
    }
    //#endregion

    //#region Initial variables
    const hmacAlgorithm = {
      name: "HMAC",
      length,
      hash: {
        name: parameters.hashAlgorithm
      }
    };
    //#endregion

    //#region Create PKCS#12 key for integrity checking
    const pkcsKey = await makePKCS12B2Key(this, parameters.hashAlgorithm, length, parameters.password, parameters.salt, parameters.iterationCount);
    //#endregion

    //#region Import HMAC key

    const hmacKey = await this.importKey("raw",
      new Uint8Array(pkcsKey),
      hmacAlgorithm,
      false,
      ["sign"]);
    //#endregion

    //#region Make signed HMAC value
    return this.sign(hmacAlgorithm, hmacKey, new Uint8Array(parameters.contentToStamp));
    //#endregion
  }

  public async verifyDataStampedWithPassword(parameters: type.CryptoEngineVerifyDataStampedWithPasswordParams): Promise<boolean> {
    //#region Check for input parameters
    ParameterError.assert(parameters,
      "password", "hashAlgorithm", "salt",
      "iterationCount", "contentToVerify", "signatureToVerify");
    //#endregion

    //#region Choose correct length for HMAC key
    let length = 0;

    switch (parameters.hashAlgorithm.toLowerCase()) {
      case "sha-1":
        length = 160;
        break;
      case "sha-256":
        length = 256;
        break;
      case "sha-384":
        length = 384;
        break;
      case "sha-512":
        length = 512;
        break;
      default:
        throw new Error(`Incorrect "parameters.hashAlgorithm" parameter: ${parameters.hashAlgorithm}`);
    }
    //#endregion

    //#region Initial variables
    const hmacAlgorithm = {
      name: "HMAC",
      length,
      hash: {
        name: parameters.hashAlgorithm
      }
    };
    //#endregion

    //#region Create PKCS#12 key for integrity checking
    const pkcsKey = await makePKCS12B2Key(this, parameters.hashAlgorithm, length, parameters.password, parameters.salt, parameters.iterationCount);
    //#endregion

    //#region Import HMAC key
    const hmacKey = await this.importKey("raw",
      new Uint8Array(pkcsKey),
      hmacAlgorithm,
      false,
      ["verify"]);
    //#endregion

    //#region Make signed HMAC value
    return this.verify(hmacAlgorithm, hmacKey, new Uint8Array(parameters.signatureToVerify), new Uint8Array(parameters.contentToVerify));
    //#endregion
  }

  public async getSignatureParameters(privateKey: CryptoKey, hashAlgorithm = "SHA-1"): Promise<type.CryptoEngineSignatureParams> {
    // Check hashing algorithm
    this.getOIDByAlgorithm({ name: hashAlgorithm }, true, "hashAlgorithm");

    // Initial variables
    const signatureAlgorithm = new AlgorithmIdentifier();

    //#region Get a "default parameters" for current algorithm
    const parameters = this.getAlgorithmParameters(privateKey.algorithm.name, "sign");
    if (!Object.keys(parameters.algorithm).length) {
      throw new Error("Parameter 'algorithm' is empty");
    }
    const algorithm = parameters.algorithm as any; // TODO remove `as any`
    algorithm.hash.name = hashAlgorithm;
    //#endregion

    //#region Fill internal structures base on "privateKey" and "hashAlgorithm"
    switch (privateKey.algorithm.name.toUpperCase()) {
      case "RSASSA-PKCS1-V1_5":
      case "ECDSA":
        signatureAlgorithm.algorithmId = this.getOIDByAlgorithm(algorithm, true);
        break;
      case "RSA-PSS":
        {
          //#region Set "saltLength" as a length (in octets) of hash function result
          switch (hashAlgorithm.toUpperCase()) {
            case "SHA-256":
              algorithm.saltLength = 32;
              break;
            case "SHA-384":
              algorithm.saltLength = 48;
              break;
            case "SHA-512":
              algorithm.saltLength = 64;
              break;
            default:
          }
          //#endregion

          //#region Fill "RSASSA_PSS_params" object
          const paramsObject: Partial<IRSASSAPSSParams> = {};

          if (hashAlgorithm.toUpperCase() !== "SHA-1") {
            const hashAlgorithmOID = this.getOIDByAlgorithm({ name: hashAlgorithm }, true, "hashAlgorithm");

            paramsObject.hashAlgorithm = new AlgorithmIdentifier({
              algorithmId: hashAlgorithmOID,
              algorithmParams: new asn1js.Null()
            });

            paramsObject.maskGenAlgorithm = new AlgorithmIdentifier({
              algorithmId: "1.2.840.113549.1.1.8", // MGF1
              algorithmParams: paramsObject.hashAlgorithm.toSchema()
            });
          }

          if (algorithm.saltLength !== 20)
            paramsObject.saltLength = algorithm.saltLength;

          const pssParameters = new RSASSAPSSParams(paramsObject);
          //#endregion

          //#region Automatically set signature algorithm
          signatureAlgorithm.algorithmId = "1.2.840.113549.1.1.10";
          signatureAlgorithm.algorithmParams = pssParameters.toSchema();
          //#endregion
        }
        break;
      default:
        throw new Error(`Unsupported signature algorithm: ${privateKey.algorithm.name}`);
    }
    //#endregion

    return {
      signatureAlgorithm,
      parameters
    };
  }

  public async signWithPrivateKey(data: BufferSource, privateKey: CryptoKey, parameters: type.CryptoEngineSignWithPrivateKeyParams): Promise<ArrayBuffer> {
    const signature = await this.sign(parameters.algorithm,
      privateKey,
      data);

    //#region Special case for ECDSA algorithm
    if (parameters.algorithm.name === "ECDSA") {
      return common.createCMSECDSASignature(signature);
    }
    //#endregion

    return signature;
  }

  public fillPublicKeyParameters(publicKeyInfo: PublicKeyInfo, signatureAlgorithm: AlgorithmIdentifier): type.CryptoEnginePublicKeyParams {
    const parameters = {} as any;

    //#region Find signer's hashing algorithm
    const shaAlgorithm = this.getHashAlgorithm(signatureAlgorithm);
    if (shaAlgorithm === EMPTY_STRING)
      throw new Error(`Unsupported signature algorithm: ${signatureAlgorithm.algorithmId}`);
    //#endregion

    //#region Get information about public key algorithm and default parameters for import
    let algorithmId: string;
    if (signatureAlgorithm.algorithmId === "1.2.840.113549.1.1.10")
      algorithmId = signatureAlgorithm.algorithmId;
    else
      algorithmId = publicKeyInfo.algorithm.algorithmId;

    const algorithmObject = this.getAlgorithmByOID(algorithmId, true);

    parameters.algorithm = this.getAlgorithmParameters(algorithmObject.name, "importKey");
    if ("hash" in parameters.algorithm.algorithm)
      parameters.algorithm.algorithm.hash.name = shaAlgorithm;

    //#region Special case for ECDSA
    if (algorithmObject.name === "ECDSA") {
      //#region Get information about named curve
      const publicKeyAlgorithm = publicKeyInfo.algorithm;
      if (!publicKeyAlgorithm.algorithmParams) {
        throw new Error("Algorithm parameters for ECDSA public key are missed");
      }
      const publicKeyAlgorithmParams = publicKeyAlgorithm.algorithmParams;
      if ("idBlock" in publicKeyAlgorithm.algorithmParams) {
        if (!((publicKeyAlgorithmParams.idBlock.tagClass === 1) && (publicKeyAlgorithmParams.idBlock.tagNumber === 6))) {
          throw new Error("Incorrect type for ECDSA public key parameters");
        }
      }

      const curveObject = this.getAlgorithmByOID(publicKeyAlgorithmParams.valueBlock.toString(), true);
      //#endregion

      parameters.algorithm.algorithm.namedCurve = curveObject.name;
    }
    //#endregion
    //#endregion

    return parameters;
  }

  public async getPublicKey(publicKeyInfo: PublicKeyInfo, signatureAlgorithm: AlgorithmIdentifier, parameters?: type.CryptoEnginePublicKeyParams): Promise<CryptoKey> {
    if (!parameters) {
      parameters = this.fillPublicKeyParameters(publicKeyInfo, signatureAlgorithm);
    }

    const publicKeyInfoBuffer = publicKeyInfo.toSchema().toBER(false);

    return this.importKey("spki",
      publicKeyInfoBuffer,
      parameters.algorithm.algorithm as Algorithm,
      true,
      parameters.algorithm.usages
    );
  }

  public async verifyWithPublicKey(data: BufferSource, signature: asn1js.BitString | asn1js.OctetString, publicKeyInfo: PublicKeyInfo, signatureAlgorithm: AlgorithmIdentifier, shaAlgorithm?: string): Promise<boolean> {
    //#region Find signer's hashing algorithm
    let publicKey: CryptoKey;
    if (!shaAlgorithm) {
      shaAlgorithm = this.getHashAlgorithm(signatureAlgorithm);
      if (!shaAlgorithm)
        throw new Error(`Unsupported signature algorithm: ${signatureAlgorithm.algorithmId}`);

      //#region Import public key
      publicKey = await this.getPublicKey(publicKeyInfo, signatureAlgorithm);
      //#endregion
    } else {
      const parameters = {} as type.CryptoEnginePublicKeyParams;

      //#region Get information about public key algorithm and default parameters for import
      let algorithmId;
      if (signatureAlgorithm.algorithmId === "1.2.840.113549.1.1.10")
        algorithmId = signatureAlgorithm.algorithmId;
      else
        algorithmId = publicKeyInfo.algorithm.algorithmId;

      const algorithmObject = this.getAlgorithmByOID(algorithmId, true);

      parameters.algorithm = this.getAlgorithmParameters(algorithmObject.name, "importKey");
      if ("hash" in parameters.algorithm.algorithm)
        (parameters.algorithm.algorithm as any).hash.name = shaAlgorithm;

      //#region Special case for ECDSA
      if (algorithmObject.name === "ECDSA") {
        //#region Get information about named curve
        let algorithmParamsChecked = false;

        if (("algorithmParams" in publicKeyInfo.algorithm) === true) {
          if ("idBlock" in publicKeyInfo.algorithm.algorithmParams) {
            if ((publicKeyInfo.algorithm.algorithmParams.idBlock.tagClass === 1) && (publicKeyInfo.algorithm.algorithmParams.idBlock.tagNumber === 6))
              algorithmParamsChecked = true;
          }
        }

        if (algorithmParamsChecked === false) {
          throw new Error("Incorrect type for ECDSA public key parameters");
        }

        const curveObject = this.getAlgorithmByOID(publicKeyInfo.algorithm.algorithmParams.valueBlock.toString(), true);
        //#endregion

        (parameters.algorithm.algorithm as any).namedCurve = curveObject.name;
      }
      //#endregion
      //#endregion

      //#region Import public key

      publicKey = await this.getPublicKey(publicKeyInfo, null as any, parameters); // TODO null!!!
      //#endregion
    }
    //#endregion

    //#region Verify signature
    //#region Get default algorithm parameters for verification
    const algorithm = this.getAlgorithmParameters(publicKey.algorithm.name, "verify");
    if ("hash" in algorithm.algorithm)
      (algorithm.algorithm as any).hash.name = shaAlgorithm;
    //#endregion

    //#region Special case for ECDSA signatures
    let signatureValue: BufferSource = signature.valueBlock.valueHexView;

    if (publicKey.algorithm.name === "ECDSA") {
      const namedCurve = ECNamedCurves.find((publicKey.algorithm as EcKeyAlgorithm).namedCurve);
      if (!namedCurve) {
        throw new Error("Unsupported named curve in use");
      }
      const asn1 = asn1js.fromBER(signatureValue);
      AsnError.assert(asn1, "Signature value");
      signatureValue = common.createECDSASignatureFromCMS(asn1.result, namedCurve.size);
    }
    //#endregion

    //#region Special case for RSA-PSS
    if (publicKey.algorithm.name === "RSA-PSS") {
      const pssParameters = new RSASSAPSSParams({ schema: signatureAlgorithm.algorithmParams });

      if ("saltLength" in pssParameters)
        (algorithm.algorithm as any).saltLength = pssParameters.saltLength;
      else
        (algorithm.algorithm as any).saltLength = 20;

      let hashAlgo = "SHA-1";

      if ("hashAlgorithm" in pssParameters) {
        const hashAlgorithm = this.getAlgorithmByOID(pssParameters.hashAlgorithm.algorithmId, true);

        hashAlgo = hashAlgorithm.name;
      }

      (algorithm.algorithm as any).hash.name = hashAlgo;
    }
    //#endregion

    return this.verify((algorithm.algorithm as any),
      publicKey,
      signatureValue,
      data,
    );
    //#endregion
  }

}

