import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import * as common from "./common";
import { PublicKeyInfo, PublicKeyInfoJson } from "./PublicKeyInfo";
import { RelativeDistinguishedNames, RelativeDistinguishedNamesJson, RelativeDistinguishedNamesSchema } from "./RelativeDistinguishedNames";
import { AlgorithmIdentifier, AlgorithmIdentifierJson } from "./AlgorithmIdentifier";
import { Attribute, AttributeJson, AttributeSchema } from "./Attribute";
import * as Schema from "./Schema";
import { CryptoEnginePublicKeyParams } from "./CryptoEngine/CryptoEngineInterface";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_BUFFER } from "./constants";

const TBS = "tbs";
const VERSION = "version";
const SUBJECT = "subject";
const SPKI = "subjectPublicKeyInfo";
const ATTRIBUTES = "attributes";
const SIGNATURE_ALGORITHM = "signatureAlgorithm";
const SIGNATURE_VALUE = "signatureValue";
const CSR_INFO = "CertificationRequestInfo";
const CSR_INFO_VERSION = `${CSR_INFO}.version`;
const CSR_INFO_SUBJECT = `${CSR_INFO}.subject`;
const CSR_INFO_SPKI = `${CSR_INFO}.subjectPublicKeyInfo`;
const CSR_INFO_ATTRS = `${CSR_INFO}.attributes`;
const CLEAR_PROPS = [
  CSR_INFO,
  CSR_INFO_VERSION,
  CSR_INFO_SUBJECT,
  CSR_INFO_SPKI,
  CSR_INFO_ATTRS,
  SIGNATURE_ALGORITHM,
  SIGNATURE_VALUE
];

export interface ICertificationRequest {
  /**
   * Value being signed
   */
  tbs: ArrayBuffer;
  /**
   * Version number. It should be 0
   */
  version: number;
  /**
   * Distinguished name of the certificate subject
   */
  subject: RelativeDistinguishedNames;
  /**
   * Information about the public key being certified
   */
  subjectPublicKeyInfo: PublicKeyInfo;
  /**
   * Collection of attributes providing additional information about the subject of the certificate
   */
  attributes?: Attribute[];

  /**
   * signature algorithm (and any associated parameters) under which the certification-request information is signed
   */
  signatureAlgorithm: AlgorithmIdentifier;
  /**
   * result of signing the certification request information with the certification request subject's private key
   */
  signatureValue: asn1js.BitString;
}

/**
 * JSON representation of {@link CertificationRequest}
 */
export interface CertificationRequestJson {
  tbs: string;
  version: number;
  subject: RelativeDistinguishedNamesJson;
  subjectPublicKeyInfo: PublicKeyInfoJson | JsonWebKey;
  attributes?: AttributeJson[];
  signatureAlgorithm: AlgorithmIdentifierJson;
  signatureValue: asn1js.BitStringJson;
}

export interface CertificationRequestInfoParameters {
  names?: {
    blockName?: string;
    CertificationRequestInfo?: string;
    CertificationRequestInfoVersion?: string;
    subject?: RelativeDistinguishedNamesSchema;
    CertificationRequestInfoAttributes?: string;
    attributes?: AttributeSchema;
  };
}

function CertificationRequestInfo(parameters: CertificationRequestInfoParameters = {}) {
  //CertificationRequestInfo ::= SEQUENCE {
  //    version       INTEGER { v1(0) } (v1,...),
  //    subject       Name,
  //    subjectPKInfo SubjectPublicKeyInfo{{ PKInfoAlgorithms }},
  //    attributes    [0] Attributes{{ CRIAttributes }}
  //}

  const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

  return (new asn1js.Sequence({
    name: (names.CertificationRequestInfo || CSR_INFO),
    value: [
      new asn1js.Integer({ name: (names.CertificationRequestInfoVersion || CSR_INFO_VERSION) }),
      RelativeDistinguishedNames.schema(names.subject || {
        names: {
          blockName: CSR_INFO_SUBJECT
        }
      }),
      PublicKeyInfo.schema({
        names: {
          blockName: CSR_INFO_SPKI
        }
      }),
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          new asn1js.Repeated({
            optional: true, // Because OpenSSL makes wrong ATTRIBUTES field
            name: (names.CertificationRequestInfoAttributes || CSR_INFO_ATTRS),
            value: Attribute.schema(names.attributes || {})
          })
        ]
      })
    ]
  }));
}

export type CertificationRequestParameters = PkiObjectParameters & Partial<ICertificationRequest>;

/**
 * Represents the CertificationRequest structure described in [RFC2986](https://datatracker.ietf.org/doc/html/rfc2986)
 *
 * @example The following example demonstrates how to parse PKCS#11 certification request
 * and verify its challenge password extension and signature value
 * ```js
 * const pkcs10 = pkijs.CertificationRequest.fromBER(pkcs10Raw);
 *
 * // Get and validate challenge password extension
 * if (pkcs10.attributes) {
 *   const attrExtensions = pkcs10.attributes.find(o => o.type === "1.2.840.113549.1.9.14"); // pkcs-9-at-extensionRequest
 *   if (attrExtensions) {
 *     const extensions = new pkijs.Extensions({ schema: attrExtensions.values[0] });
 *     for (const extension of extensions.extensions) {
 *       if (extension.extnID === "1.2.840.113549.1.9.7") { // pkcs-9-at-challengePassword
 *         const asn = asn1js.fromBER(extension.extnValue.valueBlock.valueHex);
 *         if (asn.result.valueBlock.value !== "passwordChallenge") {
 *           throw new Error("PKCS#11 certification request is invalid. Challenge password is incorrect");
 *         }
 *       }
 *     }
 *   }
 * }
 *
 * // Verify signature value
 * const ok = await pkcs10.verify();
 * if (!ok) {
 *   throw Error("PKCS#11 certification request is invalid. Signature is wrong")
 * }
 * ```
 *
 * @example The following example demonstrates how to create PKCS#11 certification request
 * ```js
 * // Get a "crypto" extension
 * const crypto = pkijs.getCrypto(true);
 *
 * const pkcs10 = new pkijs.CertificationRequest();
 *
 * pkcs10.subject.typesAndValues.push(new pkijs.AttributeTypeAndValue({
 *   type: "2.5.4.3",
 *   value: new asn1js.Utf8String({ value: "Test" })
 * }));
 *
 *
 * await pkcs10.subjectPublicKeyInfo.importKey(keys.publicKey);
 *
 * pkcs10.attributes = [];
 *
 * // Subject Alternative Name
 * const altNames = new pkijs.GeneralNames({
 *   names: [
 *     new pkijs.GeneralName({ // email
 *       type: 1,
 *       value: "email@address.com"
 *     }),
 *     new pkijs.GeneralName({ // domain
 *       type: 2,
 *       value: "www.domain.com"
 *     }),
 *   ]
 * });
 *
 * // SubjectKeyIdentifier
 * const subjectKeyIdentifier = await crypto.digest({ name: "SHA-1" }, pkcs10.subjectPublicKeyInfo.subjectPublicKey.valueBlock.valueHex);
 *
 * pkcs10.attributes.push(new pkijs.Attribute({
 *   type: "1.2.840.113549.1.9.14", // pkcs-9-at-extensionRequest
 *   values: [(new pkijs.Extensions({
 *     extensions: [
 *       new pkijs.Extension({
 *         extnID: "2.5.29.14", // id-ce-subjectKeyIdentifier
 *         critical: false,
 *         extnValue: (new asn1js.OctetString({ valueHex: subjectKeyIdentifier })).toBER(false)
 *       }),
 *       new pkijs.Extension({
 *         extnID: "2.5.29.17", // id-ce-subjectAltName
 *         critical: false,
 *         extnValue: altNames.toSchema().toBER(false)
 *       }),
 *       new pkijs.Extension({
 *         extnID: "1.2.840.113549.1.9.7", // pkcs-9-at-challengePassword
 *         critical: false,
 *         extnValue: (new asn1js.PrintableString({ value: "passwordChallenge" })).toBER(false)
 *       })
 *     ]
 *   })).toSchema()]
 * }));
 *
 * // Signing final PKCS#10 request
 * await pkcs10.sign(keys.privateKey, "SHA-256");
 *
 * const pkcs10Raw = pkcs10.toSchema(true).toBER();
 * ```
 */
export class CertificationRequest extends PkiObject implements ICertificationRequest {

  public static override CLASS_NAME = "CertificationRequest";

  public tbsView!: Uint8Array;
  /**
   * @deprecated Since version 3.0.0
   */
  public get tbs(): ArrayBuffer {
    return pvtsutils.BufferSourceConverter.toArrayBuffer(this.tbsView);
  }

  /**
   * @deprecated Since version 3.0.0
   */
  public set tbs(value: ArrayBuffer) {
    this.tbsView = new Uint8Array(value);
  }
  public version!: number;
  public subject!: RelativeDistinguishedNames;
  public subjectPublicKeyInfo!: PublicKeyInfo;
  public attributes?: Attribute[];
  public signatureAlgorithm!: AlgorithmIdentifier;
  public signatureValue!: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link CertificationRequest} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CertificationRequestParameters = {}) {
    super();

    this.tbsView = new Uint8Array(pvutils.getParametersValue(parameters, TBS, CertificationRequest.defaultValues(TBS)));
    this.version = pvutils.getParametersValue(parameters, VERSION, CertificationRequest.defaultValues(VERSION));
    this.subject = pvutils.getParametersValue(parameters, SUBJECT, CertificationRequest.defaultValues(SUBJECT));
    this.subjectPublicKeyInfo = pvutils.getParametersValue(parameters, SPKI, CertificationRequest.defaultValues(SPKI));
    if (ATTRIBUTES in parameters) {
      this.attributes = pvutils.getParametersValue(parameters, ATTRIBUTES, CertificationRequest.defaultValues(ATTRIBUTES));
    }
    this.signatureAlgorithm = pvutils.getParametersValue(parameters, SIGNATURE_ALGORITHM, CertificationRequest.defaultValues(SIGNATURE_ALGORITHM));
    this.signatureValue = pvutils.getParametersValue(parameters, SIGNATURE_VALUE, CertificationRequest.defaultValues(SIGNATURE_VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TBS): ArrayBuffer;
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof SUBJECT): RelativeDistinguishedNames;
  public static override defaultValues(memberName: typeof SPKI): PublicKeyInfo;
  public static override defaultValues(memberName: typeof ATTRIBUTES): Attribute[];
  public static override defaultValues(memberName: typeof SIGNATURE_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SIGNATURE_VALUE): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TBS:
        return EMPTY_BUFFER;
      case VERSION:
        return 0;
      case SUBJECT:
        return new RelativeDistinguishedNames();
      case SPKI:
        return new PublicKeyInfo();
      case ATTRIBUTES:
        return [];
      case SIGNATURE_ALGORITHM:
        return new AlgorithmIdentifier();
      case SIGNATURE_VALUE:
        return new asn1js.BitString();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * CertificationRequest ::= SEQUENCE {
   *    certificationRequestInfo CertificationRequestInfo,
   *    signatureAlgorithm       AlgorithmIdentifier{{ SignatureAlgorithms }},
   *    signature                BIT STRING
   * }
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{
    certificationRequestInfo?: CertificationRequestInfoParameters;
    signatureAlgorithm?: string;
    signatureValue?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      value: [
        CertificationRequestInfo(names.certificationRequestInfo || {}),
        new asn1js.Sequence({
          name: (names.signatureAlgorithm || SIGNATURE_ALGORITHM),
          value: [
            new asn1js.ObjectIdentifier(),
            new asn1js.Any({ optional: true })
          ]
        }),
        new asn1js.BitString({ name: (names.signatureValue || SIGNATURE_VALUE) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      CertificationRequest.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.tbsView = (asn1.result.CertificationRequestInfo as asn1js.Sequence).valueBeforeDecodeView;
    this.version = asn1.result[CSR_INFO_VERSION].valueBlock.valueDec;
    this.subject = new RelativeDistinguishedNames({ schema: asn1.result[CSR_INFO_SUBJECT] });
    this.subjectPublicKeyInfo = new PublicKeyInfo({ schema: asn1.result[CSR_INFO_SPKI] });
    if (CSR_INFO_ATTRS in asn1.result) {
      this.attributes = Array.from(asn1.result[CSR_INFO_ATTRS], element => new Attribute({ schema: element }));
    }
    this.signatureAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.signatureAlgorithm });
    this.signatureValue = asn1.result.signatureValue;
  }

  /**
   * Aux function making ASN1js Sequence from current TBS
   * @returns
   */
  protected encodeTBS(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [
      new asn1js.Integer({ value: this.version }),
      this.subject.toSchema(),
      this.subjectPublicKeyInfo.toSchema()
    ];

    if (ATTRIBUTES in this) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: Array.from(this.attributes || [], o => o.toSchema())
      }));
    }
    //#endregion

    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toSchema(encodeFlag = false): asn1js.Sequence {
    let tbsSchema;

    if (encodeFlag === false) {
      if (this.tbsView.byteLength === 0) { // No stored TBS part
        return CertificationRequest.schema();
      }

      const asn1 = asn1js.fromBER(this.tbsView);
      AsnError.assert(asn1, "PKCS#10 Certificate Request");

      tbsSchema = asn1.result;
    } else {
      tbsSchema = this.encodeTBS();
    }

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        tbsSchema,
        this.signatureAlgorithm.toSchema(),
        this.signatureValue
      ]
    }));
    //#endregion
  }

  public toJSON(): CertificationRequestJson {
    const object: CertificationRequestJson = {
      tbs: pvtsutils.Convert.ToHex(this.tbsView),
      version: this.version,
      subject: this.subject.toJSON(),
      subjectPublicKeyInfo: this.subjectPublicKeyInfo.toJSON(),
      signatureAlgorithm: this.signatureAlgorithm.toJSON(),
      signatureValue: this.signatureValue.toJSON(),
    };

    if (ATTRIBUTES in this) {
      object.attributes = Array.from(this.attributes || [], o => o.toJSON());
    }

    return object;
  }

  /**
   * Makes signature for current certification request
   * @param privateKey WebCrypto private key
   * @param hashAlgorithm String representing current hashing algorithm
   * @param crypto Crypto engine
   */
  async sign(privateKey: CryptoKey, hashAlgorithm = "SHA-1", crypto = common.getCrypto(true)): Promise<void> {
    // Initial checking
    if (!privateKey) {
      throw new Error("Need to provide a private key for signing");
    }

    //#region Get a "default parameters" for current algorithm and set correct signature algorithm
    const signatureParams = await crypto.getSignatureParameters(privateKey, hashAlgorithm);
    const parameters = signatureParams.parameters;
    this.signatureAlgorithm = signatureParams.signatureAlgorithm;
    //#endregion

    //#region Create TBS data for signing
    this.tbsView = new Uint8Array(this.encodeTBS().toBER());
    //#endregion

    //#region Signing TBS data on provided private key
    const signature = await crypto.signWithPrivateKey(this.tbsView, privateKey, parameters as any);
    this.signatureValue = new asn1js.BitString({ valueHex: signature });
    //#endregion
  }

  /**
   * Verify existing certification request signature
   * @param crypto Crypto engine
   * @returns Returns `true` if signature value is valid, otherwise `false`
   */
  public async verify(crypto = common.getCrypto(true)): Promise<boolean> {
    return crypto.verifyWithPublicKey(this.tbsView, this.signatureValue, this.subjectPublicKeyInfo, this.signatureAlgorithm);
  }

  /**
   * Importing public key for current certificate request
   * @param parameters
   * @param crypto Crypto engine
   * @returns WebCrypt public key
   */
  public async getPublicKey(parameters?: CryptoEnginePublicKeyParams, crypto = common.getCrypto(true)): Promise<CryptoKey> {
    return crypto.getPublicKey(this.subjectPublicKeyInfo, this.signatureAlgorithm, parameters);
  }

}

