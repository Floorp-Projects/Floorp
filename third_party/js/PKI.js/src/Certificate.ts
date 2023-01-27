import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import * as common from "./common";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { RelativeDistinguishedNames, RelativeDistinguishedNamesJson, RelativeDistinguishedNamesSchema } from "./RelativeDistinguishedNames";
import { Time, TimeJson, TimeSchema } from "./Time";
import { PublicKeyInfo, PublicKeyInfoJson, PublicKeyInfoSchema } from "./PublicKeyInfo";
import { Extension, ExtensionJson } from "./Extension";
import { Extensions, ExtensionsSchema } from "./Extensions";
import * as Schema from "./Schema";
import { id_BasicConstraints } from "./ObjectIdentifiers";
import { BasicConstraints } from "./BasicConstraints";
import { CryptoEnginePublicKeyParams } from "./CryptoEngine/CryptoEngineInterface";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";

const TBS = "tbs";
const VERSION = "version";
const SERIAL_NUMBER = "serialNumber";
const SIGNATURE = "signature";
const ISSUER = "issuer";
const NOT_BEFORE = "notBefore";
const NOT_AFTER = "notAfter";
const SUBJECT = "subject";
const SUBJECT_PUBLIC_KEY_INFO = "subjectPublicKeyInfo";
const ISSUER_UNIQUE_ID = "issuerUniqueID";
const SUBJECT_UNIQUE_ID = "subjectUniqueID";
const EXTENSIONS = "extensions";
const SIGNATURE_ALGORITHM = "signatureAlgorithm";
const SIGNATURE_VALUE = "signatureValue";
const TBS_CERTIFICATE = "tbsCertificate";
const TBS_CERTIFICATE_VERSION = `${TBS_CERTIFICATE}.${VERSION}`;
const TBS_CERTIFICATE_SERIAL_NUMBER = `${TBS_CERTIFICATE}.${SERIAL_NUMBER}`;
const TBS_CERTIFICATE_SIGNATURE = `${TBS_CERTIFICATE}.${SIGNATURE}`;
const TBS_CERTIFICATE_ISSUER = `${TBS_CERTIFICATE}.${ISSUER}`;
const TBS_CERTIFICATE_NOT_BEFORE = `${TBS_CERTIFICATE}.${NOT_BEFORE}`;
const TBS_CERTIFICATE_NOT_AFTER = `${TBS_CERTIFICATE}.${NOT_AFTER}`;
const TBS_CERTIFICATE_SUBJECT = `${TBS_CERTIFICATE}.${SUBJECT}`;
const TBS_CERTIFICATE_SUBJECT_PUBLIC_KEY = `${TBS_CERTIFICATE}.${SUBJECT_PUBLIC_KEY_INFO}`;
const TBS_CERTIFICATE_ISSUER_UNIQUE_ID = `${TBS_CERTIFICATE}.${ISSUER_UNIQUE_ID}`;
const TBS_CERTIFICATE_SUBJECT_UNIQUE_ID = `${TBS_CERTIFICATE}.${SUBJECT_UNIQUE_ID}`;
const TBS_CERTIFICATE_EXTENSIONS = `${TBS_CERTIFICATE}.${EXTENSIONS}`;
const CLEAR_PROPS = [
  TBS_CERTIFICATE,
  TBS_CERTIFICATE_VERSION,
  TBS_CERTIFICATE_SERIAL_NUMBER,
  TBS_CERTIFICATE_SIGNATURE,
  TBS_CERTIFICATE_ISSUER,
  TBS_CERTIFICATE_NOT_BEFORE,
  TBS_CERTIFICATE_NOT_AFTER,
  TBS_CERTIFICATE_SUBJECT,
  TBS_CERTIFICATE_SUBJECT_PUBLIC_KEY,
  TBS_CERTIFICATE_ISSUER_UNIQUE_ID,
  TBS_CERTIFICATE_SUBJECT_UNIQUE_ID,
  TBS_CERTIFICATE_EXTENSIONS,
  SIGNATURE_ALGORITHM,
  SIGNATURE_VALUE
];

export type TBSCertificateSchema = Schema.SchemaParameters<{
  tbsCertificateVersion?: string;
  tbsCertificateSerialNumber?: string;
  signature?: AlgorithmIdentifierSchema;
  issuer?: RelativeDistinguishedNamesSchema;
  tbsCertificateValidity?: string;
  notBefore?: TimeSchema;
  notAfter?: TimeSchema;
  subject?: RelativeDistinguishedNamesSchema;
  subjectPublicKeyInfo?: PublicKeyInfoSchema;
  tbsCertificateIssuerUniqueID?: string;
  tbsCertificateSubjectUniqueID?: string;
  extensions?: ExtensionsSchema;
}>;

function tbsCertificate(parameters: TBSCertificateSchema = {}): Schema.SchemaType {
  //TBSCertificate ::= SEQUENCE  {
  //    version         [0]  EXPLICIT Version DEFAULT v1,
  //    serialNumber         CertificateSerialNumber,
  //    signature            AlgorithmIdentifier,
  //    issuer               Name,
  //    validity             Validity,
  //    subject              Name,
  //    subjectPublicKeyInfo SubjectPublicKeyInfo,
  //    issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                         -- If present, version MUST be v2 or v3
  //    subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                         -- If present, version MUST be v2 or v3
  //    extensions      [3]  EXPLICIT Extensions OPTIONAL
  //    -- If present, version MUST be v3
  //}

  const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

  return (new asn1js.Sequence({
    name: (names.blockName || TBS_CERTIFICATE),
    value: [
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          new asn1js.Integer({ name: (names.tbsCertificateVersion || TBS_CERTIFICATE_VERSION) }) // EXPLICIT integer value
        ]
      }),
      new asn1js.Integer({ name: (names.tbsCertificateSerialNumber || TBS_CERTIFICATE_SERIAL_NUMBER) }),
      AlgorithmIdentifier.schema(names.signature || {
        names: {
          blockName: TBS_CERTIFICATE_SIGNATURE
        }
      }),
      RelativeDistinguishedNames.schema(names.issuer || {
        names: {
          blockName: TBS_CERTIFICATE_ISSUER
        }
      }),
      new asn1js.Sequence({
        name: (names.tbsCertificateValidity || "tbsCertificate.validity"),
        value: [
          Time.schema(names.notBefore || {
            names: {
              utcTimeName: TBS_CERTIFICATE_NOT_BEFORE,
              generalTimeName: TBS_CERTIFICATE_NOT_BEFORE
            }
          }),
          Time.schema(names.notAfter || {
            names: {
              utcTimeName: TBS_CERTIFICATE_NOT_AFTER,
              generalTimeName: TBS_CERTIFICATE_NOT_AFTER
            }
          })
        ]
      }),
      RelativeDistinguishedNames.schema(names.subject || {
        names: {
          blockName: TBS_CERTIFICATE_SUBJECT
        }
      }),
      PublicKeyInfo.schema(names.subjectPublicKeyInfo || {
        names: {
          blockName: TBS_CERTIFICATE_SUBJECT_PUBLIC_KEY
        }
      }),
      new asn1js.Primitive({
        name: (names.tbsCertificateIssuerUniqueID || TBS_CERTIFICATE_ISSUER_UNIQUE_ID),
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        }
      }), // IMPLICIT BIT_STRING value
      new asn1js.Primitive({
        name: (names.tbsCertificateSubjectUniqueID || TBS_CERTIFICATE_SUBJECT_UNIQUE_ID),
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 2 // [2]
        }
      }), // IMPLICIT BIT_STRING value
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 3 // [3]
        },
        value: [Extensions.schema(names.extensions || {
          names: {
            blockName: TBS_CERTIFICATE_EXTENSIONS
          }
        })]
      }) // EXPLICIT SEQUENCE value
    ]
  }));
}

export interface ICertificate {
  /**
   * ToBeSigned (TBS) part of the certificate
   */
  tbs: ArrayBuffer;
  /**
   * Version number
   */
  version: number;
  /**
   * Serial number of the certificate
   */
  serialNumber: asn1js.Integer;
  /**
   * This field contains the algorithm identifier for the algorithm used by the CA to sign the certificate
   */
  signature: AlgorithmIdentifier;
  /**
   * The issuer field identifies the entity that has signed and issued the certificate
   */
  issuer: RelativeDistinguishedNames;
  /**
   * The date on which the certificate validity period begins
   */
  notBefore: Time;
  /**
   * The date on which the certificate validity period ends
   */
  notAfter: Time;
  /**
   * The subject field identifies the entity associated with the public key stored in the subject public key field
   */
  subject: RelativeDistinguishedNames;
  /**
   * This field is used to carry the public key and identify the algorithm with which the key is used
   */
  subjectPublicKeyInfo: PublicKeyInfo;
  /**
   * The subject and issuer unique identifiers are present in the certificate to handle the possibility of reuse of subject and/or issuer names over time
   */
  issuerUniqueID?: ArrayBuffer;
  /**
   * The subject and issuer unique identifiers are present in the certificate to handle the possibility of reuse of subject and/or issuer names over time
   */
  subjectUniqueID?: ArrayBuffer;
  /**
   * If present, this field is a SEQUENCE of one or more certificate extensions
   */
  extensions?: Extension[];
  /**
   * The signatureAlgorithm field contains the identifier for the cryptographic algorithm used by the CA to sign this certificate
   */
  signatureAlgorithm: AlgorithmIdentifier;
  /**
   * The signatureValue field contains a digital signature computed upon the ASN.1 DER encoded tbsCertificate
   */
  signatureValue: asn1js.BitString;
}

/**
 * Constructor parameters for the {@link Certificate} class
 */
export type CertificateParameters = PkiObjectParameters & Partial<ICertificate>;

/**
 * Parameters for {@link Certificate} schema generation
 */
export type CertificateSchema = Schema.SchemaParameters<{
  tbsCertificate?: TBSCertificateSchema;
  signatureAlgorithm?: AlgorithmIdentifierSchema;
  signatureValue?: string;
}>;

export interface CertificateJson {
  tbs: string;
  version: number;
  serialNumber: asn1js.IntegerJson;
  signature: AlgorithmIdentifierJson;
  issuer: RelativeDistinguishedNamesJson;
  notBefore: TimeJson;
  notAfter: TimeJson;
  subject: RelativeDistinguishedNamesJson;
  subjectPublicKeyInfo: PublicKeyInfoJson | JsonWebKey;
  issuerUniqueID?: string;
  subjectUniqueID?: string;
  extensions?: ExtensionJson[];
  signatureAlgorithm: AlgorithmIdentifierJson;
  signatureValue: asn1js.BitStringJson;
}

/**
 * Represents an X.509 certificate described in [RFC5280 Section 4](https://datatracker.ietf.org/doc/html/rfc5280#section-4).
 *
 * @example The following example demonstrates how to parse X.509 Certificate
 * ```js
 * const asn1 = asn1js.fromBER(raw);
 * if (asn1.offset === -1) {
 *   throw new Error("Incorrect encoded ASN.1 data");
 * }
 *
 * const cert = new pkijs.Certificate({ schema: asn1.result });
 * ```
 *
 * @example The following example demonstrates how to create self-signed certificate
 * ```js
 * const crypto = pkijs.getCrypto(true);
 *
 * // Create certificate
 * const certificate = new pkijs.Certificate();
 * certificate.version = 2;
 * certificate.serialNumber = new asn1js.Integer({ value: 1 });
 * certificate.issuer.typesAndValues.push(new pkijs.AttributeTypeAndValue({
 *   type: "2.5.4.3", // Common name
 *   value: new asn1js.BmpString({ value: "Test" })
 * }));
 * certificate.subject.typesAndValues.push(new pkijs.AttributeTypeAndValue({
 *   type: "2.5.4.3", // Common name
 *   value: new asn1js.BmpString({ value: "Test" })
 * }));
 *
 * certificate.notBefore.value = new Date();
 * const notAfter = new Date();
 * notAfter.setUTCFullYear(notAfter.getUTCFullYear() + 1);
 * certificate.notAfter.value = notAfter;
 *
 * certificate.extensions = []; // Extensions are not a part of certificate by default, it's an optional array
 *
 * // "BasicConstraints" extension
 * const basicConstr = new pkijs.BasicConstraints({
 *   cA: true,
 *   pathLenConstraint: 3
 * });
 * certificate.extensions.push(new pkijs.Extension({
 *   extnID: "2.5.29.19",
 *   critical: false,
 *   extnValue: basicConstr.toSchema().toBER(false),
 *   parsedValue: basicConstr // Parsed value for well-known extensions
 * }));
 *
 * // "KeyUsage" extension
 * const bitArray = new ArrayBuffer(1);
 * const bitView = new Uint8Array(bitArray);
 * bitView[0] |= 0x02; // Key usage "cRLSign" flag
 * bitView[0] |= 0x04; // Key usage "keyCertSign" flag
 * const keyUsage = new asn1js.BitString({ valueHex: bitArray });
 * certificate.extensions.push(new pkijs.Extension({
 *   extnID: "2.5.29.15",
 *   critical: false,
 *   extnValue: keyUsage.toBER(false),
 *   parsedValue: keyUsage // Parsed value for well-known extensions
 * }));
 *
 * const algorithm = pkijs.getAlgorithmParameters("RSASSA-PKCS1-v1_5", "generateKey");
 * if ("hash" in algorithm.algorithm) {
 *   algorithm.algorithm.hash.name = "SHA-256";
 * }
 *
 * const keys = await crypto.generateKey(algorithm.algorithm, true, algorithm.usages);
 *
 * // Exporting public key into "subjectPublicKeyInfo" value of certificate
 * await certificate.subjectPublicKeyInfo.importKey(keys.publicKey);
 *
 * // Signing final certificate
 * await certificate.sign(keys.privateKey, "SHA-256");
 *
 * const raw = certificate.toSchema().toBER();
 * ```
 */
export class Certificate extends PkiObject implements ICertificate {

  public static override CLASS_NAME = "Certificate";

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
  public serialNumber!: asn1js.Integer;
  public signature!: AlgorithmIdentifier;
  public issuer!: RelativeDistinguishedNames;
  public notBefore!: Time;
  public notAfter!: Time;
  public subject!: RelativeDistinguishedNames;
  public subjectPublicKeyInfo!: PublicKeyInfo;
  public issuerUniqueID?: ArrayBuffer;
  public subjectUniqueID?: ArrayBuffer;
  public extensions?: Extension[];
  public signatureAlgorithm!: AlgorithmIdentifier;
  public signatureValue!: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link Certificate} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CertificateParameters = {}) {
    super();

    this.tbsView = new Uint8Array(pvutils.getParametersValue(parameters, TBS, Certificate.defaultValues(TBS)));
    this.version = pvutils.getParametersValue(parameters, VERSION, Certificate.defaultValues(VERSION));
    this.serialNumber = pvutils.getParametersValue(parameters, SERIAL_NUMBER, Certificate.defaultValues(SERIAL_NUMBER));
    this.signature = pvutils.getParametersValue(parameters, SIGNATURE, Certificate.defaultValues(SIGNATURE));
    this.issuer = pvutils.getParametersValue(parameters, ISSUER, Certificate.defaultValues(ISSUER));
    this.notBefore = pvutils.getParametersValue(parameters, NOT_BEFORE, Certificate.defaultValues(NOT_BEFORE));
    this.notAfter = pvutils.getParametersValue(parameters, NOT_AFTER, Certificate.defaultValues(NOT_AFTER));
    this.subject = pvutils.getParametersValue(parameters, SUBJECT, Certificate.defaultValues(SUBJECT));
    this.subjectPublicKeyInfo = pvutils.getParametersValue(parameters, SUBJECT_PUBLIC_KEY_INFO, Certificate.defaultValues(SUBJECT_PUBLIC_KEY_INFO));
    if (ISSUER_UNIQUE_ID in parameters) {
      this.issuerUniqueID = pvutils.getParametersValue(parameters, ISSUER_UNIQUE_ID, Certificate.defaultValues(ISSUER_UNIQUE_ID));
    }
    if (SUBJECT_UNIQUE_ID in parameters) {
      this.subjectUniqueID = pvutils.getParametersValue(parameters, SUBJECT_UNIQUE_ID, Certificate.defaultValues(SUBJECT_UNIQUE_ID));
    }
    if (EXTENSIONS in parameters) {
      this.extensions = pvutils.getParametersValue(parameters, EXTENSIONS, Certificate.defaultValues(EXTENSIONS));
    }
    this.signatureAlgorithm = pvutils.getParametersValue(parameters, SIGNATURE_ALGORITHM, Certificate.defaultValues(SIGNATURE_ALGORITHM));
    this.signatureValue = pvutils.getParametersValue(parameters, SIGNATURE_VALUE, Certificate.defaultValues(SIGNATURE_VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Return default values for all class members
   * @param memberName String name for a class member
   * @returns Predefined default value
   */
  public static override defaultValues(memberName: typeof TBS): ArrayBuffer;
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof SERIAL_NUMBER): asn1js.Integer;
  public static override defaultValues(memberName: typeof SIGNATURE): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ISSUER): RelativeDistinguishedNames;
  public static override defaultValues(memberName: typeof NOT_BEFORE): Time;
  public static override defaultValues(memberName: typeof NOT_AFTER): Time;
  public static override defaultValues(memberName: typeof SUBJECT): RelativeDistinguishedNames;
  public static override defaultValues(memberName: typeof SUBJECT_PUBLIC_KEY_INFO): PublicKeyInfo;
  public static override defaultValues(memberName: typeof ISSUER_UNIQUE_ID): ArrayBuffer;
  public static override defaultValues(memberName: typeof SUBJECT_UNIQUE_ID): ArrayBuffer;
  public static override defaultValues(memberName: typeof EXTENSIONS): Extension[];
  public static override defaultValues(memberName: typeof SIGNATURE_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SIGNATURE_VALUE): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TBS:
        return EMPTY_BUFFER;
      case VERSION:
        return 0;
      case SERIAL_NUMBER:
        return new asn1js.Integer();
      case SIGNATURE:
        return new AlgorithmIdentifier();
      case ISSUER:
        return new RelativeDistinguishedNames();
      case NOT_BEFORE:
        return new Time();
      case NOT_AFTER:
        return new Time();
      case SUBJECT:
        return new RelativeDistinguishedNames();
      case SUBJECT_PUBLIC_KEY_INFO:
        return new PublicKeyInfo();
      case ISSUER_UNIQUE_ID:
        return EMPTY_BUFFER;
      case SUBJECT_UNIQUE_ID:
        return EMPTY_BUFFER;
      case EXTENSIONS:
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
   * Certificate ::= SEQUENCE  {
   *    tbsCertificate       TBSCertificate,
   *    signatureAlgorithm   AlgorithmIdentifier,
   *    signatureValue       BIT STRING  }
   *
   * TBSCertificate ::= SEQUENCE  {
   *     version         [0]  EXPLICIT Version DEFAULT v1,
   *     serialNumber         CertificateSerialNumber,
   *     signature            AlgorithmIdentifier,
   *     issuer               Name,
   *     validity             Validity,
   *     subject              Name,
   *     subjectPublicKeyInfo SubjectPublicKeyInfo,
   *     issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
   *                           -- If present, version MUST be v2 or v3
   *     subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
   *                           -- If present, version MUST be v2 or v3
   *     extensions      [3]  EXPLICIT Extensions OPTIONAL
   *                           -- If present, version MUST be v3
   *     }
   *
   * Version ::= INTEGER  {  v1(0), v2(1), v3(2)  }
   *
   * CertificateSerialNumber ::= INTEGER
   *
   * Validity ::= SEQUENCE {
   *     notBefore      Time,
   *     notAfter       Time }
   *
   * Time ::= CHOICE {
   *     utcTime        UTCTime,
   *     generalTime    GeneralizedTime }
   *
   * UniqueIdentifier ::= BIT STRING
   *
   * SubjectPublicKeyInfo ::= SEQUENCE  {
   *     algorithm            AlgorithmIdentifier,
   *     subjectPublicKey     BIT STRING  }
   *
   * Extensions ::= SEQUENCE SIZE (1..MAX) OF Extension
   *
   * Extension ::= SEQUENCE  {
   *     extnID      OBJECT IDENTIFIER,
   *     critical    BOOLEAN DEFAULT FALSE,
   *     extnValue   OCTET STRING
   *                 -- contains the DER encoding of an ASN.1 value
   *                 -- corresponding to the extension type identified
   *                 -- by extnID
   *     }
   *```
   */
  public static override schema(parameters: CertificateSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        tbsCertificate(names.tbsCertificate),
        AlgorithmIdentifier.schema(names.signatureAlgorithm || {
          names: {
            blockName: SIGNATURE_ALGORITHM
          }
        }),
        new asn1js.BitString({ name: (names.signatureValue || SIGNATURE_VALUE) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      Certificate.schema({
        names: {
          tbsCertificate: {
            names: {
              extensions: {
                names: {
                  extensions: TBS_CERTIFICATE_EXTENSIONS
                }
              }
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);
    //#endregion

    //#region Get internal properties from parsed schema
    this.tbsView = (asn1.result.tbsCertificate as asn1js.Sequence).valueBeforeDecodeView;

    if (TBS_CERTIFICATE_VERSION in asn1.result)
      this.version = asn1.result[TBS_CERTIFICATE_VERSION].valueBlock.valueDec;
    this.serialNumber = asn1.result[TBS_CERTIFICATE_SERIAL_NUMBER];
    this.signature = new AlgorithmIdentifier({ schema: asn1.result[TBS_CERTIFICATE_SIGNATURE] });
    this.issuer = new RelativeDistinguishedNames({ schema: asn1.result[TBS_CERTIFICATE_ISSUER] });
    this.notBefore = new Time({ schema: asn1.result[TBS_CERTIFICATE_NOT_BEFORE] });
    this.notAfter = new Time({ schema: asn1.result[TBS_CERTIFICATE_NOT_AFTER] });
    this.subject = new RelativeDistinguishedNames({ schema: asn1.result[TBS_CERTIFICATE_SUBJECT] });
    this.subjectPublicKeyInfo = new PublicKeyInfo({ schema: asn1.result[TBS_CERTIFICATE_SUBJECT_PUBLIC_KEY] });
    if (TBS_CERTIFICATE_ISSUER_UNIQUE_ID in asn1.result)
      this.issuerUniqueID = asn1.result[TBS_CERTIFICATE_ISSUER_UNIQUE_ID].valueBlock.valueHex;
    if (TBS_CERTIFICATE_SUBJECT_UNIQUE_ID in asn1.result)
      this.subjectUniqueID = asn1.result[TBS_CERTIFICATE_SUBJECT_UNIQUE_ID].valueBlock.valueHex;
    if (TBS_CERTIFICATE_EXTENSIONS in asn1.result)
      this.extensions = Array.from(asn1.result[TBS_CERTIFICATE_EXTENSIONS], element => new Extension({ schema: element }));

    this.signatureAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.signatureAlgorithm });
    this.signatureValue = asn1.result.signatureValue;
    //#endregion
  }

  /**
   * Creates ASN.1 schema for existing values of TBS part for the certificate
   * @returns ASN.1 SEQUENCE
   */
  public encodeTBS(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if ((VERSION in this) && (this.version !== Certificate.defaultValues(VERSION))) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          new asn1js.Integer({ value: this.version }) // EXPLICIT integer value
        ]
      }));
    }

    outputArray.push(this.serialNumber);
    outputArray.push(this.signature.toSchema());
    outputArray.push(this.issuer.toSchema());

    outputArray.push(new asn1js.Sequence({
      value: [
        this.notBefore.toSchema(),
        this.notAfter.toSchema()
      ]
    }));

    outputArray.push(this.subject.toSchema());
    outputArray.push(this.subjectPublicKeyInfo.toSchema());

    if (this.issuerUniqueID) {
      outputArray.push(new asn1js.Primitive({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        valueHex: this.issuerUniqueID
      }));
    }
    if (this.subjectUniqueID) {
      outputArray.push(new asn1js.Primitive({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 2 // [2]
        },
        valueHex: this.subjectUniqueID
      }));
    }

    if (this.extensions) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 3 // [3]
        },
        value: [new asn1js.Sequence({
          value: Array.from(this.extensions, o => o.toSchema())
        })]
      }));
    }
    //#endregion

    //#region Create and return output sequence
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toSchema(encodeFlag = false): asn1js.Sequence {
    let tbsSchema: asn1js.AsnType;

    // Decode stored TBS value
    if (encodeFlag === false) {
      if (!this.tbsView.byteLength) { // No stored certificate TBS part
        return Certificate.schema().value[0];
      }

      const asn1 = asn1js.fromBER(this.tbsView);
      AsnError.assert(asn1, "TBS Certificate");

      tbsSchema = asn1.result;
    } else {
      // Create TBS schema via assembling from TBS parts
      tbsSchema = this.encodeTBS();
    }

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        tbsSchema,
        this.signatureAlgorithm.toSchema(),
        this.signatureValue
      ]
    }));
  }

  public toJSON(): CertificateJson {
    const res: CertificateJson = {
      tbs: pvtsutils.Convert.ToHex(this.tbsView),
      version: this.version,
      serialNumber: this.serialNumber.toJSON(),
      signature: this.signature.toJSON(),
      issuer: this.issuer.toJSON(),
      notBefore: this.notBefore.toJSON(),
      notAfter: this.notAfter.toJSON(),
      subject: this.subject.toJSON(),
      subjectPublicKeyInfo: this.subjectPublicKeyInfo.toJSON(),
      signatureAlgorithm: this.signatureAlgorithm.toJSON(),
      signatureValue: this.signatureValue.toJSON(),
    };

    if ((VERSION in this) && (this.version !== Certificate.defaultValues(VERSION))) {
      res.version = this.version;
    }

    if (this.issuerUniqueID) {
      res.issuerUniqueID = pvtsutils.Convert.ToHex(this.issuerUniqueID);
    }

    if (this.subjectUniqueID) {
      res.subjectUniqueID = pvtsutils.Convert.ToHex(this.subjectUniqueID);
    }

    if (this.extensions) {
      res.extensions = Array.from(this.extensions, o => o.toJSON());
    }

    return res;
  }

  /**
   * Importing public key for current certificate
   * @param parameters Public key export parameters
   * @param crypto Crypto engine
   * @returns WebCrypto public key
   */
  public async getPublicKey(parameters?: CryptoEnginePublicKeyParams, crypto = common.getCrypto(true)): Promise<CryptoKey> {
    return crypto.getPublicKey(this.subjectPublicKeyInfo, this.signatureAlgorithm, parameters);
  }

  /**
   * Get hash value for subject public key (default SHA-1)
   * @param hashAlgorithm Hashing algorithm name
   * @param crypto Crypto engine
   * @returns Computed hash value from `Certificate.tbsCertificate.subjectPublicKeyInfo.subjectPublicKey`
   */
  public async getKeyHash(hashAlgorithm = "SHA-1", crypto = common.getCrypto(true)): Promise<ArrayBuffer> {
    return crypto.digest({ name: hashAlgorithm }, this.subjectPublicKeyInfo.subjectPublicKey.valueBlock.valueHexView);
  }

  /**
   * Make a signature for current value from TBS section
   * @param privateKey Private key for SUBJECT_PUBLIC_KEY_INFO structure
   * @param hashAlgorithm Hashing algorithm
   * @param crypto Crypto engine
   */
  public async sign(privateKey: CryptoKey, hashAlgorithm = "SHA-1", crypto = common.getCrypto(true)): Promise<void> {
    // Initial checking
    if (!privateKey) {
      throw new Error("Need to provide a private key for signing");
    }

    // Get a "default parameters" for current algorithm and set correct signature algorithm
    const signatureParameters = await crypto.getSignatureParameters(privateKey, hashAlgorithm);
    const parameters = signatureParameters.parameters;
    this.signature = signatureParameters.signatureAlgorithm;
    this.signatureAlgorithm = signatureParameters.signatureAlgorithm;

    // Create TBS data for signing
    this.tbsView = new Uint8Array(this.encodeTBS().toBER());

    // Signing TBS data on provided private key
    // TODO remove any
    const signature = await crypto.signWithPrivateKey(this.tbsView, privateKey, parameters as any);
    this.signatureValue = new asn1js.BitString({ valueHex: signature });
  }

  /**
   * Verifies the certificate signature
   * @param issuerCertificate
   * @param crypto Crypto engine
   */
  public async verify(issuerCertificate?: Certificate, crypto = common.getCrypto(true)): Promise<boolean> {
    let subjectPublicKeyInfo: PublicKeyInfo | undefined;

    // Set correct SUBJECT_PUBLIC_KEY_INFO value
    if (issuerCertificate) {
      subjectPublicKeyInfo = issuerCertificate.subjectPublicKeyInfo;
    } else if (this.issuer.isEqual(this.subject)) {
      // Self-signed certificate
      subjectPublicKeyInfo = this.subjectPublicKeyInfo;
    }

    if (!(subjectPublicKeyInfo instanceof PublicKeyInfo)) {
      throw new Error("Please provide issuer certificate as a parameter");
    }

    return crypto.verifyWithPublicKey(this.tbsView, this.signatureValue, subjectPublicKeyInfo, this.signatureAlgorithm);
  }

}

/**
 * Check CA flag for the certificate
 * @param cert Certificate to find CA flag for
 * @returns Returns {@link Certificate} if `cert` is CA certificate otherwise return `null`
 */
export function checkCA(cert: Certificate, signerCert: Certificate | null = null): Certificate | null {
  //#region Do not include signer's certificate
  if (signerCert && cert.issuer.isEqual(signerCert.issuer) && cert.serialNumber.isEqual(signerCert.serialNumber)) {
    return null;
  }
  //#endregion

  let isCA = false;

  if (cert.extensions) {
    for (const extension of cert.extensions) {
      if (extension.extnID === id_BasicConstraints && extension.parsedValue instanceof BasicConstraints) {
        if (extension.parsedValue.cA) {
          isCA = true;
          break;
        }
      }
    }
  }

  if (isCA) {
    return cert;
  }

  return null;
}
