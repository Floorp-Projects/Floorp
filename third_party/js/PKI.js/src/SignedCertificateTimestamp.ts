import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as bs from "bytestreamjs";
import * as common from "./common";
import { PublicKeyInfo } from "./PublicKeyInfo";
import * as Schema from "./Schema";
import { AlgorithmIdentifier } from "./AlgorithmIdentifier";
import { Certificate } from "./Certificate";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";
import { SignedCertificateTimestampList } from "./SignedCertificateTimestampList";
import { id_SignedCertificateTimestampList } from "./ObjectIdentifiers";

const VERSION = "version";
const LOG_ID = "logID";
const EXTENSIONS = "extensions";
const TIMESTAMP = "timestamp";
const HASH_ALGORITHM = "hashAlgorithm";
const SIGNATURE_ALGORITHM = "signatureAlgorithm";
const SIGNATURE = "signature";

const NONE = "none";
const MD5 = "md5";
const SHA1 = "sha1";
const SHA224 = "sha224";
const SHA256 = "sha256";
const SHA384 = "sha384";
const SHA512 = "sha512";
const ANONYMOUS = "anonymous";
const RSA = "rsa";
const DSA = "dsa";
const ECDSA = "ecdsa";

export interface ISignedCertificateTimestamp {
  version: number;
  logID: ArrayBuffer;
  timestamp: Date;
  extensions: ArrayBuffer;
  hashAlgorithm: string;
  signatureAlgorithm: string;
  signature: Schema.SchemaType;
}

export interface SignedCertificateTimestampJson {
  version: number;
  logID: string;
  timestamp: Date;
  extensions: string;
  hashAlgorithm: string;
  signatureAlgorithm: string;
  signature: Schema.SchemaType;
}

export type SignedCertificateTimestampParameters = PkiObjectParameters & Partial<ISignedCertificateTimestamp> & { stream?: bs.SeqStream; };

export interface Log {
  /**
   * Identifier of the CT Log encoded in BASE-64 format
   */
  log_id: string;
  /**
   * Public key of the CT Log encoded in BASE-64 format
   */
  key: string;
}

export class SignedCertificateTimestamp extends PkiObject implements ISignedCertificateTimestamp {

  public static override CLASS_NAME = "SignedCertificateTimestamp";

  public version!: number;
  public logID!: ArrayBuffer;
  public timestamp!: Date;
  public extensions!: ArrayBuffer;
  public hashAlgorithm!: string;
  public signatureAlgorithm!: string;
  public signature: asn1js.BaseBlock;

  /**
   * Initializes a new instance of the {@link SignedCertificateTimestamp} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SignedCertificateTimestampParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, SignedCertificateTimestamp.defaultValues(VERSION));
    this.logID = pvutils.getParametersValue(parameters, LOG_ID, SignedCertificateTimestamp.defaultValues(LOG_ID));
    this.timestamp = pvutils.getParametersValue(parameters, TIMESTAMP, SignedCertificateTimestamp.defaultValues(TIMESTAMP));
    this.extensions = pvutils.getParametersValue(parameters, EXTENSIONS, SignedCertificateTimestamp.defaultValues(EXTENSIONS));
    this.hashAlgorithm = pvutils.getParametersValue(parameters, HASH_ALGORITHM, SignedCertificateTimestamp.defaultValues(HASH_ALGORITHM));
    this.signatureAlgorithm = pvutils.getParametersValue(parameters, SIGNATURE_ALGORITHM, SignedCertificateTimestamp.defaultValues(SIGNATURE_ALGORITHM));
    this.signature = pvutils.getParametersValue(parameters, SIGNATURE, SignedCertificateTimestamp.defaultValues(SIGNATURE));

    if ("stream" in parameters && parameters.stream) {
      this.fromStream(parameters.stream);
    }

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof LOG_ID): ArrayBuffer;
  public static override defaultValues(memberName: typeof EXTENSIONS): ArrayBuffer;
  public static override defaultValues(memberName: typeof TIMESTAMP): Date;
  public static override defaultValues(memberName: typeof HASH_ALGORITHM): string;
  public static override defaultValues(memberName: typeof SIGNATURE_ALGORITHM): string;
  public static override defaultValues(memberName: typeof SIGNATURE): Schema.SchemaType;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case LOG_ID:
      case EXTENSIONS:
        return EMPTY_BUFFER;
      case TIMESTAMP:
        return new Date(0);
      case HASH_ALGORITHM:
      case SIGNATURE_ALGORITHM:
        return EMPTY_STRING;
      case SIGNATURE:
        return new asn1js.Any();
      default:
        return super.defaultValues(memberName);
    }
  }

  public fromSchema(schema: Schema.SchemaType): void {
    if ((schema instanceof asn1js.RawData) === false)
      throw new Error("Object's schema was not verified against input data for SignedCertificateTimestamp");

    const seqStream = new bs.SeqStream({
      stream: new bs.ByteStream({
        buffer: schema.data
      })
    });

    this.fromStream(seqStream);
  }

  /**
   * Converts SeqStream data into current class
   * @param stream
   */
  public fromStream(stream: bs.SeqStream): void {
    const blockLength = stream.getUint16();

    this.version = (stream.getBlock(1))[0];

    if (this.version === 0) {
      this.logID = (new Uint8Array(stream.getBlock(32))).buffer.slice(0);
      this.timestamp = new Date(pvutils.utilFromBase(new Uint8Array(stream.getBlock(8)), 8));

      //#region Extensions
      const extensionsLength = stream.getUint16();
      this.extensions = (new Uint8Array(stream.getBlock(extensionsLength))).buffer.slice(0);
      //#endregion

      //#region Hash algorithm
      switch ((stream.getBlock(1))[0]) {
        case 0:
          this.hashAlgorithm = NONE;
          break;
        case 1:
          this.hashAlgorithm = MD5;
          break;
        case 2:
          this.hashAlgorithm = SHA1;
          break;
        case 3:
          this.hashAlgorithm = SHA224;
          break;
        case 4:
          this.hashAlgorithm = SHA256;
          break;
        case 5:
          this.hashAlgorithm = SHA384;
          break;
        case 6:
          this.hashAlgorithm = SHA512;
          break;
        default:
          throw new Error("Object's stream was not correct for SignedCertificateTimestamp");
      }
      //#endregion

      //#region Signature algorithm
      switch ((stream.getBlock(1))[0]) {
        case 0:
          this.signatureAlgorithm = ANONYMOUS;
          break;
        case 1:
          this.signatureAlgorithm = RSA;
          break;
        case 2:
          this.signatureAlgorithm = DSA;
          break;
        case 3:
          this.signatureAlgorithm = ECDSA;
          break;
        default:
          throw new Error("Object's stream was not correct for SignedCertificateTimestamp");
      }
      //#endregion

      //#region Signature
      const signatureLength = stream.getUint16();
      const signatureData = new Uint8Array(stream.getBlock(signatureLength)).buffer.slice(0);

      const asn1 = asn1js.fromBER(signatureData);
      AsnError.assert(asn1, "SignedCertificateTimestamp");
      this.signature = asn1.result;
      //#endregion

      if (blockLength !== (47 + extensionsLength + signatureLength)) {
        throw new Error("Object's stream was not correct for SignedCertificateTimestamp");
      }
    }
  }

  public toSchema(): asn1js.RawData {
    const stream = this.toStream();

    return new asn1js.RawData({ data: stream.stream.buffer });
  }

  /**
   * Converts current object to SeqStream data
   * @returns SeqStream object
   */
  public toStream(): bs.SeqStream {
    const stream = new bs.SeqStream();

    stream.appendUint16(47 + this.extensions.byteLength + this.signature.valueBeforeDecodeView.byteLength);
    stream.appendChar(this.version);
    stream.appendView(new Uint8Array(this.logID));

    const timeBuffer = new ArrayBuffer(8);
    const timeView = new Uint8Array(timeBuffer);

    const baseArray = pvutils.utilToBase(this.timestamp.valueOf(), 8);
    timeView.set(new Uint8Array(baseArray), 8 - baseArray.byteLength);

    stream.appendView(timeView);
    stream.appendUint16(this.extensions.byteLength);

    if (this.extensions.byteLength)
      stream.appendView(new Uint8Array(this.extensions));

    let _hashAlgorithm;

    switch (this.hashAlgorithm.toLowerCase()) {
      case NONE:
        _hashAlgorithm = 0;
        break;
      case MD5:
        _hashAlgorithm = 1;
        break;
      case SHA1:
        _hashAlgorithm = 2;
        break;
      case SHA224:
        _hashAlgorithm = 3;
        break;
      case SHA256:
        _hashAlgorithm = 4;
        break;
      case SHA384:
        _hashAlgorithm = 5;
        break;
      case SHA512:
        _hashAlgorithm = 6;
        break;
      default:
        throw new Error(`Incorrect data for hashAlgorithm: ${this.hashAlgorithm}`);
    }

    stream.appendChar(_hashAlgorithm);

    let _signatureAlgorithm;

    switch (this.signatureAlgorithm.toLowerCase()) {
      case ANONYMOUS:
        _signatureAlgorithm = 0;
        break;
      case RSA:
        _signatureAlgorithm = 1;
        break;
      case DSA:
        _signatureAlgorithm = 2;
        break;
      case ECDSA:
        _signatureAlgorithm = 3;
        break;
      default:
        throw new Error(`Incorrect data for signatureAlgorithm: ${this.signatureAlgorithm}`);
    }

    stream.appendChar(_signatureAlgorithm);

    const _signature = this.signature.toBER(false);

    stream.appendUint16(_signature.byteLength);
    stream.appendView(new Uint8Array(_signature));

    return stream;
  }

  public toJSON(): SignedCertificateTimestampJson {
    return {
      version: this.version,
      logID: pvutils.bufferToHexCodes(this.logID),
      timestamp: this.timestamp,
      extensions: pvutils.bufferToHexCodes(this.extensions),
      hashAlgorithm: this.hashAlgorithm,
      signatureAlgorithm: this.signatureAlgorithm,
      signature: this.signature.toJSON()
    };
  }

  /**
   * Verify SignedCertificateTimestamp for specific input data
   * @param logs Array of objects with information about each CT Log (like here: https://ct.grahamedgecombe.com/logs.json)
   * @param data Data to verify signature against. Could be encoded Certificate or encoded PreCert
   * @param dataType Type = 0 (data is encoded Certificate), type = 1 (data is encoded PreCert)
   * @param crypto Crypto engine
   */
  async verify(logs: Log[], data: ArrayBuffer, dataType = 0, crypto = common.getCrypto(true)): Promise<boolean> {
    //#region Initial variables
    const logId = pvutils.toBase64(pvutils.arrayBufferToString(this.logID));

    let publicKeyBase64 = null;

    const stream = new bs.SeqStream();
    //#endregion

    //#region Found and init public key
    for (const log of logs) {
      if (log.log_id === logId) {
        publicKeyBase64 = log.key;
        break;
      }
    }

    if (!publicKeyBase64) {
      throw new Error(`Public key not found for CT with logId: ${logId}`);
    }

    const pki = pvutils.stringToArrayBuffer(pvutils.fromBase64(publicKeyBase64));
    const publicKeyInfo = PublicKeyInfo.fromBER(pki);
    //#endregion

    //#region Initialize signed data block
    stream.appendChar(0x00); // sct_version
    stream.appendChar(0x00); // signature_type = certificate_timestamp

    const timeBuffer = new ArrayBuffer(8);
    const timeView = new Uint8Array(timeBuffer);

    const baseArray = pvutils.utilToBase(this.timestamp.valueOf(), 8);
    timeView.set(new Uint8Array(baseArray), 8 - baseArray.byteLength);

    stream.appendView(timeView);

    stream.appendUint16(dataType);

    if (dataType === 0)
      stream.appendUint24(data.byteLength);

    stream.appendView(new Uint8Array(data));

    stream.appendUint16(this.extensions.byteLength);

    if (this.extensions.byteLength !== 0)
      stream.appendView(new Uint8Array(this.extensions));
    //#endregion

    //#region Perform verification
    return crypto.verifyWithPublicKey(
      stream.buffer.slice(0, stream.length),
      new asn1js.OctetString({ valueHex: this.signature.toBER(false) }),
      publicKeyInfo,
      { algorithmId: EMPTY_STRING } as AlgorithmIdentifier,
      "SHA-256"
    );
    //#endregion
  }

}

export interface Log {
  /**
   * Identifier of the CT Log encoded in BASE-64 format
   */
  log_id: string;
  /**
   * Public key of the CT Log encoded in BASE-64 format
   */
  key: string;
}

/**
 * Verify SignedCertificateTimestamp for specific certificate content
 * @param certificate Certificate for which verification would be performed
 * @param issuerCertificate Certificate of the issuer of target certificate
 * @param logs Array of objects with information about each CT Log (like here: https://ct.grahamedgecombe.com/logs.json)
 * @param index Index of SignedCertificateTimestamp inside SignedCertificateTimestampList (for -1 would verify all)
 * @param crypto Crypto engine
 * @return Array of verification results
 */
export async function verifySCTsForCertificate(certificate: Certificate, issuerCertificate: Certificate, logs: Log[], index = (-1), crypto = common.getCrypto(true)) {
  let parsedValue: SignedCertificateTimestampList | null = null;

  const stream = new bs.SeqStream();

  //#region Remove certificate extension
  for (let i = 0; certificate.extensions && i < certificate.extensions.length; i++) {
    switch (certificate.extensions[i].extnID) {
      case id_SignedCertificateTimestampList:
        {
          parsedValue = certificate.extensions[i].parsedValue;

          if (!parsedValue || parsedValue.timestamps.length === 0)
            throw new Error("Nothing to verify in the certificate");

          certificate.extensions.splice(i, 1);
        }
        break;
      default:
    }
  }
  //#endregion

  //#region Check we do have what to verify
  if (parsedValue === null)
    throw new Error("No SignedCertificateTimestampList extension in the specified certificate");
  //#endregion

  //#region Prepare modifier TBS value
  const tbs = certificate.encodeTBS().toBER();
  //#endregion

  //#region Initialize "issuer_key_hash" value
  const issuerId = await crypto.digest({ name: "SHA-256" }, new Uint8Array(issuerCertificate.subjectPublicKeyInfo.toSchema().toBER(false)));
  //#endregion

  //#region Make final "PreCert" value
  stream.appendView(new Uint8Array(issuerId));
  stream.appendUint24(tbs.byteLength);
  stream.appendView(new Uint8Array(tbs));

  const preCert = stream.stream.slice(0, stream.length);
  //#endregion

  //#region Call verification function for specified index
  if (index === (-1)) {
    const verifyArray = [];

    for (const timestamp of parsedValue.timestamps) {
      const verifyResult = await timestamp.verify(logs, preCert.buffer, 1, crypto);
      verifyArray.push(verifyResult);
    }

    return verifyArray;
  }

  if (index >= parsedValue.timestamps.length)
    index = (parsedValue.timestamps.length - 1);

  return [await parsedValue.timestamps[index].verify(logs, preCert.buffer, 1, crypto)];
  //#endregion
}

