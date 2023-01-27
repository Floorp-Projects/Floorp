import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import * as common from "./common";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { RelativeDistinguishedNames, RelativeDistinguishedNamesJson, RelativeDistinguishedNamesSchema } from "./RelativeDistinguishedNames";
import { Time, TimeJson, TimeSchema } from "./Time";
import { RevokedCertificate, RevokedCertificateJson } from "./RevokedCertificate";
import { Extensions, ExtensionsJson, ExtensionsSchema } from "./Extensions";
import * as Schema from "./Schema";
import { Certificate } from "./Certificate";
import { PublicKeyInfo } from "./PublicKeyInfo";
import { id_AuthorityInfoAccess, id_AuthorityKeyIdentifier, id_BaseCRLNumber, id_CertificateIssuer, id_CRLNumber, id_CRLReason, id_FreshestCRL, id_InvalidityDate, id_IssuerAltName, id_IssuingDistributionPoint } from "./ObjectIdentifiers";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_BUFFER } from "./constants";

const TBS = "tbs";
const VERSION = "version";
const SIGNATURE = "signature";
const ISSUER = "issuer";
const THIS_UPDATE = "thisUpdate";
const NEXT_UPDATE = "nextUpdate";
const REVOKED_CERTIFICATES = "revokedCertificates";
const CRL_EXTENSIONS = "crlExtensions";
const SIGNATURE_ALGORITHM = "signatureAlgorithm";
const SIGNATURE_VALUE = "signatureValue";
const TBS_CERT_LIST = "tbsCertList";
const TBS_CERT_LIST_VERSION = `${TBS_CERT_LIST}.version`;
const TBS_CERT_LIST_SIGNATURE = `${TBS_CERT_LIST}.signature`;
const TBS_CERT_LIST_ISSUER = `${TBS_CERT_LIST}.issuer`;
const TBS_CERT_LIST_THIS_UPDATE = `${TBS_CERT_LIST}.thisUpdate`;
const TBS_CERT_LIST_NEXT_UPDATE = `${TBS_CERT_LIST}.nextUpdate`;
const TBS_CERT_LIST_REVOKED_CERTIFICATES = `${TBS_CERT_LIST}.revokedCertificates`;
const TBS_CERT_LIST_EXTENSIONS = `${TBS_CERT_LIST}.extensions`;
const CLEAR_PROPS = [
  TBS_CERT_LIST,
  TBS_CERT_LIST_VERSION,
  TBS_CERT_LIST_SIGNATURE,
  TBS_CERT_LIST_ISSUER,
  TBS_CERT_LIST_THIS_UPDATE,
  TBS_CERT_LIST_NEXT_UPDATE,
  TBS_CERT_LIST_REVOKED_CERTIFICATES,
  TBS_CERT_LIST_EXTENSIONS,
  SIGNATURE_ALGORITHM,
  SIGNATURE_VALUE
];

export interface ICertificateRevocationList {
  tbs: ArrayBuffer;
  version: number;
  signature: AlgorithmIdentifier;
  issuer: RelativeDistinguishedNames;
  thisUpdate: Time;
  nextUpdate?: Time;
  revokedCertificates?: RevokedCertificate[];
  crlExtensions?: Extensions;
  signatureAlgorithm: AlgorithmIdentifier;
  signatureValue: asn1js.BitString;
}

export type TBSCertListSchema = Schema.SchemaParameters<{
  tbsCertListVersion?: string;
  signature?: AlgorithmIdentifierSchema;
  issuer?: RelativeDistinguishedNamesSchema;
  tbsCertListThisUpdate?: TimeSchema;
  tbsCertListNextUpdate?: TimeSchema;
  tbsCertListRevokedCertificates?: string;
  crlExtensions?: ExtensionsSchema;
}>;

export interface CertificateRevocationListJson {
  tbs: string;
  version: number;
  signature: AlgorithmIdentifierJson;
  issuer: RelativeDistinguishedNamesJson;
  thisUpdate: TimeJson;
  nextUpdate?: TimeJson;
  revokedCertificates?: RevokedCertificateJson[];
  crlExtensions?: ExtensionsJson;
  signatureAlgorithm: AlgorithmIdentifierJson;
  signatureValue: asn1js.BitStringJson;
}

function tbsCertList(parameters: TBSCertListSchema = {}): Schema.SchemaType {
  //TBSCertList ::= SEQUENCE  {
  //    version                 Version OPTIONAL,
  //                                 -- if present, MUST be v2
  //    signature               AlgorithmIdentifier,
  //    issuer                  Name,
  //    thisUpdate              Time,
  //    nextUpdate              Time OPTIONAL,
  //    revokedCertificates     SEQUENCE OF SEQUENCE  {
  //        userCertificate         CertificateSerialNumber,
  //        revocationDate          Time,
  //        crlEntryExtensions      Extensions OPTIONAL
  //        -- if present, version MUST be v2
  //    }  OPTIONAL,
  //    crlExtensions           [0]  EXPLICIT Extensions OPTIONAL
  //    -- if present, version MUST be v2
  //}
  const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

  return (new asn1js.Sequence({
    name: (names.blockName || TBS_CERT_LIST),
    value: [
      new asn1js.Integer({
        optional: true,
        name: (names.tbsCertListVersion || TBS_CERT_LIST_VERSION),
        value: 2
      }), // EXPLICIT integer value (v2)
      AlgorithmIdentifier.schema(names.signature || {
        names: {
          blockName: TBS_CERT_LIST_SIGNATURE
        }
      }),
      RelativeDistinguishedNames.schema(names.issuer || {
        names: {
          blockName: TBS_CERT_LIST_ISSUER
        }
      }),
      Time.schema(names.tbsCertListThisUpdate || {
        names: {
          utcTimeName: TBS_CERT_LIST_THIS_UPDATE,
          generalTimeName: TBS_CERT_LIST_THIS_UPDATE
        }
      }),
      Time.schema(names.tbsCertListNextUpdate || {
        names: {
          utcTimeName: TBS_CERT_LIST_NEXT_UPDATE,
          generalTimeName: TBS_CERT_LIST_NEXT_UPDATE
        }
      }, true),
      new asn1js.Sequence({
        optional: true,
        value: [
          new asn1js.Repeated({
            name: (names.tbsCertListRevokedCertificates || TBS_CERT_LIST_REVOKED_CERTIFICATES),
            value: new asn1js.Sequence({
              value: [
                new asn1js.Integer(),
                Time.schema(),
                Extensions.schema({}, true)
              ]
            })
          })
        ]
      }),
      new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [Extensions.schema(names.crlExtensions || {
          names: {
            blockName: TBS_CERT_LIST_EXTENSIONS
          }
        })]
      }) // EXPLICIT SEQUENCE value
    ]
  }));
}

export type CertificateRevocationListParameters = PkiObjectParameters & Partial<ICertificateRevocationList>;

export interface CertificateRevocationListVerifyParams {
  issuerCertificate?: Certificate;
  publicKeyInfo?: PublicKeyInfo;
}

const WELL_KNOWN_EXTENSIONS = [
  id_AuthorityKeyIdentifier,
  id_IssuerAltName,
  id_CRLNumber,
  id_BaseCRLNumber,
  id_IssuingDistributionPoint,
  id_FreshestCRL,
  id_AuthorityInfoAccess,
  id_CRLReason,
  id_InvalidityDate,
  id_CertificateIssuer,
];
/**
 * Represents the CertificateRevocationList structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class CertificateRevocationList extends PkiObject implements ICertificateRevocationList {

  public static override CLASS_NAME = "CertificateRevocationList";

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
  public signature!: AlgorithmIdentifier;
  public issuer!: RelativeDistinguishedNames;
  public thisUpdate!: Time;
  public nextUpdate?: Time;
  public revokedCertificates?: RevokedCertificate[];
  public crlExtensions?: Extensions;
  public signatureAlgorithm!: AlgorithmIdentifier;
  public signatureValue!: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link CertificateRevocationList} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CertificateRevocationListParameters = {}) {
    super();

    this.tbsView = new Uint8Array(pvutils.getParametersValue(parameters, TBS, CertificateRevocationList.defaultValues(TBS)));
    this.version = pvutils.getParametersValue(parameters, VERSION, CertificateRevocationList.defaultValues(VERSION));
    this.signature = pvutils.getParametersValue(parameters, SIGNATURE, CertificateRevocationList.defaultValues(SIGNATURE));
    this.issuer = pvutils.getParametersValue(parameters, ISSUER, CertificateRevocationList.defaultValues(ISSUER));
    this.thisUpdate = pvutils.getParametersValue(parameters, THIS_UPDATE, CertificateRevocationList.defaultValues(THIS_UPDATE));
    if (NEXT_UPDATE in parameters) {
      this.nextUpdate = pvutils.getParametersValue(parameters, NEXT_UPDATE, CertificateRevocationList.defaultValues(NEXT_UPDATE));
    }
    if (REVOKED_CERTIFICATES in parameters) {
      this.revokedCertificates = pvutils.getParametersValue(parameters, REVOKED_CERTIFICATES, CertificateRevocationList.defaultValues(REVOKED_CERTIFICATES));
    }
    if (CRL_EXTENSIONS in parameters) {
      this.crlExtensions = pvutils.getParametersValue(parameters, CRL_EXTENSIONS, CertificateRevocationList.defaultValues(CRL_EXTENSIONS));
    }
    this.signatureAlgorithm = pvutils.getParametersValue(parameters, SIGNATURE_ALGORITHM, CertificateRevocationList.defaultValues(SIGNATURE_ALGORITHM));
    this.signatureValue = pvutils.getParametersValue(parameters, SIGNATURE_VALUE, CertificateRevocationList.defaultValues(SIGNATURE_VALUE));

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
  public static override defaultValues(memberName: typeof SIGNATURE): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ISSUER): RelativeDistinguishedNames;
  public static override defaultValues(memberName: typeof THIS_UPDATE): Time;
  public static override defaultValues(memberName: typeof NEXT_UPDATE): Time;
  public static override defaultValues(memberName: typeof REVOKED_CERTIFICATES): RevokedCertificate[];
  public static override defaultValues(memberName: typeof CRL_EXTENSIONS): Extensions;
  public static override defaultValues(memberName: typeof SIGNATURE_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SIGNATURE_VALUE): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TBS:
        return EMPTY_BUFFER;
      case VERSION:
        return 0;
      case SIGNATURE:
        return new AlgorithmIdentifier();
      case ISSUER:
        return new RelativeDistinguishedNames();
      case THIS_UPDATE:
        return new Time();
      case NEXT_UPDATE:
        return new Time();
      case REVOKED_CERTIFICATES:
        return [];
      case CRL_EXTENSIONS:
        return new Extensions();
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
   * CertificateList ::= SEQUENCE  {
   *    tbsCertList          TBSCertList,
   *    signatureAlgorithm   AlgorithmIdentifier,
   *    signatureValue       BIT STRING  }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    tbsCertListVersion?: string;
    signature?: AlgorithmIdentifierSchema;
    issuer?: RelativeDistinguishedNamesSchema;
    tbsCertListThisUpdate?: TimeSchema;
    tbsCertListNextUpdate?: TimeSchema;
    tbsCertListRevokedCertificates?: string;
    crlExtensions?: ExtensionsSchema;
    signatureAlgorithm?: AlgorithmIdentifierSchema;
    signatureValue?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || "CertificateList"),
      value: [
        tbsCertList(parameters),
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

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      CertificateRevocationList.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.tbsView = (asn1.result.tbsCertList as asn1js.Sequence).valueBeforeDecodeView;

    if (TBS_CERT_LIST_VERSION in asn1.result) {
      this.version = asn1.result[TBS_CERT_LIST_VERSION].valueBlock.valueDec;
    }
    this.signature = new AlgorithmIdentifier({ schema: asn1.result[TBS_CERT_LIST_SIGNATURE] });
    this.issuer = new RelativeDistinguishedNames({ schema: asn1.result[TBS_CERT_LIST_ISSUER] });
    this.thisUpdate = new Time({ schema: asn1.result[TBS_CERT_LIST_THIS_UPDATE] });
    if (TBS_CERT_LIST_NEXT_UPDATE in asn1.result) {
      this.nextUpdate = new Time({ schema: asn1.result[TBS_CERT_LIST_NEXT_UPDATE] });
    }
    if (TBS_CERT_LIST_REVOKED_CERTIFICATES in asn1.result) {
      this.revokedCertificates = Array.from(asn1.result[TBS_CERT_LIST_REVOKED_CERTIFICATES], element => new RevokedCertificate({ schema: element }));
    }
    if (TBS_CERT_LIST_EXTENSIONS in asn1.result) {
      this.crlExtensions = new Extensions({ schema: asn1.result[TBS_CERT_LIST_EXTENSIONS] });
    }

    this.signatureAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.signatureAlgorithm });
    this.signatureValue = asn1.result.signatureValue;
    //#endregion
  }

  protected encodeTBS() {
    //#region Create array for output sequence
    const outputArray: any[] = [];

    if (this.version !== CertificateRevocationList.defaultValues(VERSION)) {
      outputArray.push(new asn1js.Integer({ value: this.version }));
    }

    outputArray.push(this.signature.toSchema());
    outputArray.push(this.issuer.toSchema());
    outputArray.push(this.thisUpdate.toSchema());

    if (this.nextUpdate) {
      outputArray.push(this.nextUpdate.toSchema());
    }

    if (this.revokedCertificates) {
      outputArray.push(new asn1js.Sequence({
        value: Array.from(this.revokedCertificates, o => o.toSchema())
      }));
    }

    if (this.crlExtensions) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          this.crlExtensions.toSchema()
        ]
      }));
    }
    //#endregion

    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  /**
   * Convert current object to asn1js object and set correct values
   * @returns asn1js object
   */
  public toSchema(encodeFlag = false) {
    //#region Decode stored TBS value
    let tbsSchema;

    if (!encodeFlag) {
      if (!this.tbsView.byteLength) { // No stored TBS part
        return CertificateRevocationList.schema();
      }

      const asn1 = asn1js.fromBER(this.tbsView);
      AsnError.assert(asn1, "TBS Certificate Revocation List");
      tbsSchema = asn1.result;
    }
    //#endregion
    //#region Create TBS schema via assembling from TBS parts
    else {
      tbsSchema = this.encodeTBS();
    }
    //#endregion

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

  public toJSON(): CertificateRevocationListJson {
    const res: CertificateRevocationListJson = {
      tbs: pvtsutils.Convert.ToHex(this.tbsView),
      version: this.version,
      signature: this.signature.toJSON(),
      issuer: this.issuer.toJSON(),
      thisUpdate: this.thisUpdate.toJSON(),
      signatureAlgorithm: this.signatureAlgorithm.toJSON(),
      signatureValue: this.signatureValue.toJSON()
    };

    if (this.version !== CertificateRevocationList.defaultValues(VERSION))
      res.version = this.version;

    if (this.nextUpdate) {
      res.nextUpdate = this.nextUpdate.toJSON();
    }

    if (this.revokedCertificates) {
      res.revokedCertificates = Array.from(this.revokedCertificates, o => o.toJSON());
    }

    if (this.crlExtensions) {
      res.crlExtensions = this.crlExtensions.toJSON();
    }

    return res;
  }

  /**
   * Returns `true` if supplied certificate is revoked, otherwise `false`
   * @param certificate
   */
  public isCertificateRevoked(certificate: Certificate): boolean {
    // Check that issuer of the input certificate is the same with issuer of this CRL
    if (!this.issuer.isEqual(certificate.issuer)) {
      return false;
    }

    // Check that there are revoked certificates in this CRL
    if (!this.revokedCertificates) {
      return false;
    }

    // Search for input certificate in revoked certificates array
    for (const revokedCertificate of this.revokedCertificates) {
      if (revokedCertificate.userCertificate.isEqual(certificate.serialNumber)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Make a signature for existing CRL data
   * @param privateKey Private key for "subjectPublicKeyInfo" structure
   * @param hashAlgorithm Hashing algorithm. Default SHA-1
   * @param crypto Crypto engine
   */
  public async sign(privateKey: CryptoKey, hashAlgorithm = "SHA-1", crypto = common.getCrypto(true)): Promise<void> {
    // Get a private key from function parameter
    if (!privateKey) {
      throw new Error("Need to provide a private key for signing");
    }

    //#region Get a "default parameters" for current algorithm and set correct signature algorithm
    const signatureParameters = await crypto.getSignatureParameters(privateKey, hashAlgorithm);
    const { parameters } = signatureParameters;
    this.signature = signatureParameters.signatureAlgorithm;
    this.signatureAlgorithm = signatureParameters.signatureAlgorithm;
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
   * Verify existing signature
   * @param parameters
   * @param crypto Crypto engine
   */
  public async verify(parameters: CertificateRevocationListVerifyParams = {}, crypto = common.getCrypto(true)): Promise<boolean> {
    let subjectPublicKeyInfo: PublicKeyInfo | undefined;

    //#region Get information about CRL issuer certificate
    if (parameters.issuerCertificate) { // "issuerCertificate" must be of type "Certificate"
      subjectPublicKeyInfo = parameters.issuerCertificate.subjectPublicKeyInfo;

      // The CRL issuer name and "issuerCertificate" subject name are not equal
      if (!this.issuer.isEqual(parameters.issuerCertificate.subject)) {
        return false;
      }
    }

    //#region In case if there is only public key during verification
    if (parameters.publicKeyInfo) {
      subjectPublicKeyInfo = parameters.publicKeyInfo; // Must be of type "PublicKeyInfo"
    }
    //#endregion

    if (!subjectPublicKeyInfo) {
      throw new Error("Issuer's certificate must be provided as an input parameter");
    }
    //#endregion

    //#region Check the CRL for unknown critical extensions
    if (this.crlExtensions) {
      for (const extension of this.crlExtensions.extensions) {
        if (extension.critical) {
          // We can not be sure that unknown extension has no value for CRL signature
          if (!WELL_KNOWN_EXTENSIONS.includes(extension.extnID))
            return false;
        }
      }
    }
    //#endregion

    return crypto.verifyWithPublicKey(this.tbsView, this.signatureValue, subjectPublicKeyInfo, this.signatureAlgorithm);
  }

}
