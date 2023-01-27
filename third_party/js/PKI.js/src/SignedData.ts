import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import * as common from "./common";
import { AlgorithmIdentifier, AlgorithmIdentifierJson } from "./AlgorithmIdentifier";
import { EncapsulatedContentInfo, EncapsulatedContentInfoJson, EncapsulatedContentInfoSchema } from "./EncapsulatedContentInfo";
import { Certificate, checkCA } from "./Certificate";
import { CertificateRevocationList, CertificateRevocationListJson } from "./CertificateRevocationList";
import { OtherRevocationInfoFormat, OtherRevocationInfoFormatJson } from "./OtherRevocationInfoFormat";
import { SignerInfo, SignerInfoJson } from "./SignerInfo";
import { CertificateSet, CertificateSetItem, CertificateSetItemJson } from "./CertificateSet";
import { RevocationInfoChoices, RevocationInfoChoicesSchema } from "./RevocationInfoChoices";
import { IssuerAndSerialNumber } from "./IssuerAndSerialNumber";
import { TSTInfo } from "./TSTInfo";
import { CertificateChainValidationEngine, CertificateChainValidationEngineParameters, FindIssuerCallback, FindOriginCallback } from "./CertificateChainValidationEngine";
import { BasicOCSPResponse, BasicOCSPResponseJson } from "./BasicOCSPResponse";
import { OtherCertificateFormat } from "./OtherCertificateFormat";
import { AttributeCertificateV1 } from "./AttributeCertificateV1";
import { AttributeCertificateV2 } from "./AttributeCertificateV2";
import * as Schema from "./Schema";
import { id_ContentType_Data, id_eContentType_TSTInfo, id_PKIX_OCSP_Basic } from "./ObjectIdentifiers";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";
import { ICryptoEngine } from "./CryptoEngine/CryptoEngineInterface";

export type SignedDataCRL = CertificateRevocationList | OtherRevocationInfoFormat;
export type SignedDataCRLJson = CertificateRevocationListJson | OtherRevocationInfoFormatJson;

const VERSION = "version";
const DIGEST_ALGORITHMS = "digestAlgorithms";
const ENCAP_CONTENT_INFO = "encapContentInfo";
const CERTIFICATES = "certificates";
const CRLS = "crls";
const SIGNER_INFOS = "signerInfos";
const OCSPS = "ocsps";
const SIGNED_DATA = "SignedData";
const SIGNED_DATA_VERSION = `${SIGNED_DATA}.${VERSION}`;
const SIGNED_DATA_DIGEST_ALGORITHMS = `${SIGNED_DATA}.${DIGEST_ALGORITHMS}`;
const SIGNED_DATA_ENCAP_CONTENT_INFO = `${SIGNED_DATA}.${ENCAP_CONTENT_INFO}`;
const SIGNED_DATA_CERTIFICATES = `${SIGNED_DATA}.${CERTIFICATES}`;
const SIGNED_DATA_CRLS = `${SIGNED_DATA}.${CRLS}`;
const SIGNED_DATA_SIGNER_INFOS = `${SIGNED_DATA}.${SIGNER_INFOS}`;
const CLEAR_PROPS = [
  SIGNED_DATA_VERSION,
  SIGNED_DATA_DIGEST_ALGORITHMS,
  SIGNED_DATA_ENCAP_CONTENT_INFO,
  SIGNED_DATA_CERTIFICATES,
  SIGNED_DATA_CRLS,
  SIGNED_DATA_SIGNER_INFOS
];

export interface ISignedData {
  version: number;
  digestAlgorithms: AlgorithmIdentifier[];
  encapContentInfo: EncapsulatedContentInfo;
  certificates?: CertificateSetItem[];
  crls?: SignedDataCRL[];
  ocsps?: BasicOCSPResponse[];
  signerInfos: SignerInfo[];
}

export interface SignedDataJson {
  version: number;
  digestAlgorithms: AlgorithmIdentifierJson[];
  encapContentInfo: EncapsulatedContentInfoJson;
  certificates?: CertificateSetItemJson[];
  crls?: SignedDataCRLJson[];
  ocsps?: BasicOCSPResponseJson[];
  signerInfos: SignerInfoJson[];
}

export type SignedDataParameters = PkiObjectParameters & Partial<ISignedData>;

export interface SignedDataVerifyParams {
  signer?: number;
  data?: ArrayBuffer;
  trustedCerts?: Certificate[];
  checkDate?: Date;
  checkChain?: boolean;
  passedWhenNotRevValues?: boolean;
  extendedMode?: boolean;
  findOrigin?: FindOriginCallback | null;
  findIssuer?: FindIssuerCallback | null;
}

export interface SignedDataVerifyErrorParams {
  message: string;
  date?: Date;
  code?: number;
  timestampSerial?: ArrayBuffer | null;
  signatureVerified?: boolean | null;
  signerCertificate?: Certificate | null;
  signerCertificateVerified?: boolean | null;
  certificatePath?: Certificate[];
}

export interface SignedDataVerifyResult {
  message: string;
  date?: Date;
  code?: number;
  timestampSerial?: ArrayBuffer | null;
  signatureVerified?: boolean | null;
  signerCertificate?: Certificate | null;
  signerCertificateVerified?: boolean | null;
  certificatePath: Certificate[];
}

export class SignedDataVerifyError extends Error implements SignedDataVerifyResult {

  public date: Date;
  public code: number;
  public signatureVerified: boolean | null;
  public signerCertificate: Certificate | null;
  public signerCertificateVerified: boolean | null;
  public timestampSerial: ArrayBuffer | null;
  public certificatePath: Certificate[];

  constructor({
    message,
    code = 0,
    date = new Date(),
    signatureVerified = null,
    signerCertificate = null,
    signerCertificateVerified = null,
    timestampSerial = null,
    certificatePath = [],
  }: SignedDataVerifyErrorParams) {
    super(message);
    this.name = "SignedDataVerifyError";

    this.date = date;
    this.code = code;
    this.timestampSerial = timestampSerial;
    this.signatureVerified = signatureVerified;
    this.signerCertificate = signerCertificate;
    this.signerCertificateVerified = signerCertificateVerified;
    this.certificatePath = certificatePath;

  }
}

/**
 * Represents the SignedData structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 *
 * @example The following example demonstrates how to create and sign CMS Signed Data
 * ```js
 * // Create a new CMS Signed Data
 * const cmsSigned = new pkijs.SignedData({
 *   encapContentInfo: new pkijs.EncapsulatedContentInfo({
 *     eContentType: pkijs.ContentInfo.DATA,, // "data" content type
 *     eContent: new asn1js.OctetString({ valueHex: buffer })
 *   }),
 *   signerInfos: [
 *     new pkijs.SignerInfo({
 *       sid: new pkijs.IssuerAndSerialNumber({
 *         issuer: cert.issuer,
 *         serialNumber: cert.serialNumber
 *       })
 *     })
 *   ],
 *   // Signer certificate for chain validation
 *   certificates: [cert]
 * });
 *
 * await cmsSigned.sign(keys.privateKey, 0, "SHA-256");
 *
 * // Add Signed Data to Content Info
 * const cms = new pkijs.ContentInfo({
 *   contentType: pkijs.ContentInfo.SIGNED_DATA,,
 *   content: cmsSigned.toSchema(true),
 * });
 *
 * // Encode CMS to ASN.1
 * const cmsRaw = cms.toSchema().toBER();
 * ```
 *
 * @example The following example demonstrates how to verify CMS Signed Data
 * ```js
 * // Parse CMS and detect it's Signed Data
 * const cms = pkijs.ContentInfo.fromBER(cmsRaw);
 * if (cms.contentType !== pkijs.ContentInfo.SIGNED_DATA) {
 *   throw new Error("CMS is not Signed Data");
 * }
 *
 * // Read Signed Data
 * const signedData = new pkijs.SignedData({ schema: cms.content });
 *
 * // Verify Signed Data signature
 * const ok = await signedData.verify({
 *   signer: 0,
 *   checkChain: true,
 *   trustedCerts: [trustedCert],
 * });
 *
 * if (!ok) {
 *   throw new Error("CMS signature is invalid")
 * }
 * ```
 */
export class SignedData extends PkiObject implements ISignedData {

  public static override CLASS_NAME = "SignedData";

  public static ID_DATA: typeof id_ContentType_Data = id_ContentType_Data;

  public version!: number;
  public digestAlgorithms!: AlgorithmIdentifier[];
  public encapContentInfo!: EncapsulatedContentInfo;
  public certificates?: CertificateSetItem[];
  public crls?: SignedDataCRL[];
  public ocsps?: BasicOCSPResponse[];
  public signerInfos!: SignerInfo[];

  /**
   * Initializes a new instance of the {@link SignedData} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SignedDataParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, SignedData.defaultValues(VERSION));
    this.digestAlgorithms = pvutils.getParametersValue(parameters, DIGEST_ALGORITHMS, SignedData.defaultValues(DIGEST_ALGORITHMS));
    this.encapContentInfo = pvutils.getParametersValue(parameters, ENCAP_CONTENT_INFO, SignedData.defaultValues(ENCAP_CONTENT_INFO));
    if (CERTIFICATES in parameters) {
      this.certificates = pvutils.getParametersValue(parameters, CERTIFICATES, SignedData.defaultValues(CERTIFICATES));
    }
    if (CRLS in parameters) {
      this.crls = pvutils.getParametersValue(parameters, CRLS, SignedData.defaultValues(CRLS));
    }
    if (OCSPS in parameters) {
      this.ocsps = pvutils.getParametersValue(parameters, OCSPS, SignedData.defaultValues(OCSPS));
    }
    this.signerInfos = pvutils.getParametersValue(parameters, SIGNER_INFOS, SignedData.defaultValues(SIGNER_INFOS));

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
  public static override defaultValues(memberName: typeof DIGEST_ALGORITHMS): AlgorithmIdentifier[];
  public static override defaultValues(memberName: typeof ENCAP_CONTENT_INFO): EncapsulatedContentInfo;
  public static override defaultValues(memberName: typeof CERTIFICATES): CertificateSetItem[];
  public static override defaultValues(memberName: typeof CRLS): SignedDataCRL[];
  public static override defaultValues(memberName: typeof OCSPS): BasicOCSPResponse[];
  public static override defaultValues(memberName: typeof SIGNER_INFOS): SignerInfo[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case DIGEST_ALGORITHMS:
        return [];
      case ENCAP_CONTENT_INFO:
        return new EncapsulatedContentInfo();
      case CERTIFICATES:
        return [];
      case CRLS:
        return [];
      case OCSPS:
        return [];
      case SIGNER_INFOS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compare values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case VERSION:
        return (memberValue === SignedData.defaultValues(VERSION));
      case ENCAP_CONTENT_INFO:
        return EncapsulatedContentInfo.compareWithDefault("eContentType", memberValue.eContentType) &&
          EncapsulatedContentInfo.compareWithDefault("eContent", memberValue.eContent);
      case DIGEST_ALGORITHMS:
      case CERTIFICATES:
      case CRLS:
      case OCSPS:
      case SIGNER_INFOS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SignedData ::= SEQUENCE {
   *    version CMSVersion,
   *    digestAlgorithms DigestAlgorithmIdentifiers,
   *    encapContentInfo EncapsulatedContentInfo,
   *    certificates [0] IMPLICIT CertificateSet OPTIONAL,
   *    crls [1] IMPLICIT RevocationInfoChoices OPTIONAL,
   *    signerInfos SignerInfos }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    digestAlgorithms?: string;
    encapContentInfo?: EncapsulatedContentInfoSchema;
    certificates?: string;
    crls?: RevocationInfoChoicesSchema;
    signerInfos?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    if (names.optional === undefined) {
      names.optional = false;
    }

    return (new asn1js.Sequence({
      name: (names.blockName || SIGNED_DATA),
      optional: names.optional,
      value: [
        new asn1js.Integer({ name: (names.version || SIGNED_DATA_VERSION) }),
        new asn1js.Set({
          value: [
            new asn1js.Repeated({
              name: (names.digestAlgorithms || SIGNED_DATA_DIGEST_ALGORITHMS),
              value: AlgorithmIdentifier.schema()
            })
          ]
        }),
        EncapsulatedContentInfo.schema(names.encapContentInfo || {
          names: {
            blockName: SIGNED_DATA_ENCAP_CONTENT_INFO
          }
        }),
        new asn1js.Constructed({
          name: (names.certificates || SIGNED_DATA_CERTIFICATES),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: CertificateSet.schema().valueBlock.value
        }), // IMPLICIT CertificateSet
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: RevocationInfoChoices.schema(names.crls || {
            names: {
              crls: SIGNED_DATA_CRLS
            }
          }).valueBlock.value
        }), // IMPLICIT RevocationInfoChoices
        new asn1js.Set({
          value: [
            new asn1js.Repeated({
              name: (names.signerInfos || SIGNED_DATA_SIGNER_INFOS),
              value: SignerInfo.schema()
            })
          ]
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      SignedData.schema()
    );
    AsnError.assertSchema(asn1, this.className);
    //#endregion

    //#region Get internal properties from parsed schema
    this.version = asn1.result[SIGNED_DATA_VERSION].valueBlock.valueDec;

    if (SIGNED_DATA_DIGEST_ALGORITHMS in asn1.result) // Could be empty SET of digest algorithms
      this.digestAlgorithms = Array.from(asn1.result[SIGNED_DATA_DIGEST_ALGORITHMS], algorithm => new AlgorithmIdentifier({ schema: algorithm }));

    this.encapContentInfo = new EncapsulatedContentInfo({ schema: asn1.result[SIGNED_DATA_ENCAP_CONTENT_INFO] });

    if (SIGNED_DATA_CERTIFICATES in asn1.result) {
      const certificateSet = new CertificateSet({
        schema: new asn1js.Set({
          value: asn1.result[SIGNED_DATA_CERTIFICATES].valueBlock.value
        })
      });
      this.certificates = certificateSet.certificates.slice(0); // Copy all just for making comfortable access
    }

    if (SIGNED_DATA_CRLS in asn1.result) {
      this.crls = Array.from(asn1.result[SIGNED_DATA_CRLS], (crl: Schema.SchemaType) => {
        if (crl.idBlock.tagClass === 1)
          return new CertificateRevocationList({ schema: crl });

        //#region Create SEQUENCE from [1]
        crl.idBlock.tagClass = 1; // UNIVERSAL
        crl.idBlock.tagNumber = 16; // SEQUENCE
        //#endregion

        return new OtherRevocationInfoFormat({ schema: crl });
      });
    }

    if (SIGNED_DATA_SIGNER_INFOS in asn1.result) // Could be empty SET SignerInfos
      this.signerInfos = Array.from(asn1.result[SIGNED_DATA_SIGNER_INFOS], signerInfoSchema => new SignerInfo({ schema: signerInfoSchema }));
    //#endregion
  }

  public toSchema(encodeFlag = false): Schema.SchemaType {
    //#region Create array for output sequence
    const outputArray = [];

    // IF ((certificates is present) AND
    //  (any certificates with a type of other are present)) OR
    //  ((crls is present) AND
    //  (any crls with a type of other are present))
    // THEN version MUST be 5
    // ELSE
    //  IF (certificates is present) AND
    //    (any version 2 attribute certificates are present)
    //  THEN version MUST be 4
    //  ELSE
    //    IF ((certificates is present) AND
    //      (any version 1 attribute certificates are present)) OR
    //      (any SignerInfo structures are version 3) OR
    //      (encapContentInfo eContentType is other than id-data)
    //    THEN version MUST be 3
    //    ELSE version MUST be 1
    if ((this.certificates && this.certificates.length && this.certificates.some(o => o instanceof OtherCertificateFormat))
      || (this.crls && this.crls.length && this.crls.some(o => o instanceof OtherRevocationInfoFormat))) {
      this.version = 5;
    } else if (this.certificates && this.certificates.length && this.certificates.some(o => o instanceof AttributeCertificateV2)) {
      this.version = 4;
    } else if ((this.certificates && this.certificates.length && this.certificates.some(o => o instanceof AttributeCertificateV1))
      || this.signerInfos.some(o => o.version === 3)
      || this.encapContentInfo.eContentType !== SignedData.ID_DATA) {
      this.version = 3;
    } else {
      this.version = 1;
    }

    outputArray.push(new asn1js.Integer({ value: this.version }));

    //#region Create array of digest algorithms
    outputArray.push(new asn1js.Set({
      value: Array.from(this.digestAlgorithms, algorithm => algorithm.toSchema())
    }));
    //#endregion

    outputArray.push(this.encapContentInfo.toSchema());

    if (this.certificates) {
      const certificateSet = new CertificateSet({ certificates: this.certificates });
      const certificateSetSchema = certificateSet.toSchema();

      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 0
        },
        value: certificateSetSchema.valueBlock.value
      }));
    }

    if (this.crls) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: Array.from(this.crls, crl => {
          if (crl instanceof OtherRevocationInfoFormat) {
            const crlSchema = crl.toSchema();

            crlSchema.idBlock.tagClass = 3;
            crlSchema.idBlock.tagNumber = 1;

            return crlSchema;
          }

          return crl.toSchema(encodeFlag);
        })
      }));
    }

    //#region Create array of signer infos
    outputArray.push(new asn1js.Set({
      value: Array.from(this.signerInfos, signerInfo => signerInfo.toSchema())
    }));
    //#endregion
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): SignedDataJson {
    const res: SignedDataJson = {
      version: this.version,
      digestAlgorithms: Array.from(this.digestAlgorithms, algorithm => algorithm.toJSON()),
      encapContentInfo: this.encapContentInfo.toJSON(),
      signerInfos: Array.from(this.signerInfos, signerInfo => signerInfo.toJSON()),
    };

    if (this.certificates) {
      res.certificates = Array.from(this.certificates, certificate => certificate.toJSON());
    }

    if (this.crls) {
      res.crls = Array.from(this.crls, crl => crl.toJSON());
    }


    return res;
  }

  public verify(params?: SignedDataVerifyParams & { extendedMode?: false; }, crypto?: ICryptoEngine): Promise<boolean>;
  public verify(params: SignedDataVerifyParams & { extendedMode: true; }, crypto?: ICryptoEngine): Promise<SignedDataVerifyResult>;
  public async verify({
    signer = (-1),
    data = (EMPTY_BUFFER),
    trustedCerts = [],
    checkDate = (new Date()),
    checkChain = false,
    passedWhenNotRevValues = false,
    extendedMode = false,
    findOrigin = null,
    findIssuer = null
  }: SignedDataVerifyParams = {}, crypto = common.getCrypto(true)): Promise<boolean | SignedDataVerifyResult> {
    let signerCert: Certificate | null = null;
    let timestampSerial: ArrayBuffer | null = null;
    try {
      //#region Global variables
      let messageDigestValue = EMPTY_BUFFER;
      let shaAlgorithm = EMPTY_STRING;
      let certificatePath: Certificate[] = [];
      //#endregion

      //#region Get a signer number
      const signerInfo = this.signerInfos[signer];
      if (!signerInfo) {
        throw new SignedDataVerifyError({
          date: checkDate,
          code: 1,
          message: "Unable to get signer by supplied index",
        });
      }
      //#endregion

      //#region Check that certificates field was included in signed data
      if (!this.certificates) {
        throw new SignedDataVerifyError({
          date: checkDate,
          code: 2,
          message: "No certificates attached to this signed data",
        });
      }
      //#endregion

      //#region Find a certificate for specified signer

      if (signerInfo.sid instanceof IssuerAndSerialNumber) {
        for (const certificate of this.certificates) {
          if (!(certificate instanceof Certificate))
            continue;

          if ((certificate.issuer.isEqual(signerInfo.sid.issuer)) &&
            (certificate.serialNumber.isEqual(signerInfo.sid.serialNumber))) {
            signerCert = certificate;
            break;
          }
        }
      } else { // Find by SubjectKeyIdentifier
        const sid = signerInfo.sid;
        const keyId = sid.idBlock.isConstructed
          ? sid.valueBlock.value[0].valueBlock.valueHex // EXPLICIT OCTET STRING
          : sid.valueBlock.valueHex; // IMPLICIT OCTET STRING

        for (const certificate of this.certificates) {
          if (!(certificate instanceof Certificate)) {
            continue;
          }

          const digest = await crypto.digest({ name: "sha-1" }, certificate.subjectPublicKeyInfo.subjectPublicKey.valueBlock.valueHexView);
          if (pvutils.isEqualBuffer(digest, keyId)) {
            signerCert = certificate;
            break;
          }
        }
      }

      if (!signerCert) {
        throw new SignedDataVerifyError({
          date: checkDate,
          code: 3,
          message: "Unable to find signer certificate",
        });
      }
      //#endregion

      //#region Verify internal digest in case of "tSTInfo" content type
      if (this.encapContentInfo.eContentType === id_eContentType_TSTInfo) {
        //#region Check "eContent" presence
        if (!this.encapContentInfo.eContent) {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 15,
            message: "Error during verification: TSTInfo eContent is empty",
            signatureVerified: null,
            signerCertificate: signerCert,
            timestampSerial,
            signerCertificateVerified: true
          });
        }
        //#endregion

        //#region Initialize TST_INFO value
        let tstInfo: TSTInfo;

        try {
          tstInfo = TSTInfo.fromBER(this.encapContentInfo.eContent.valueBlock.valueHexView);
        }
        catch (ex) {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 15,
            message: "Error during verification: TSTInfo wrong ASN.1 schema ",
            signatureVerified: null,
            signerCertificate: signerCert,
            timestampSerial,
            signerCertificateVerified: true
          });
        }
        //#endregion

        //#region Change "checkDate" and append "timestampSerial"
        checkDate = tstInfo.genTime;
        timestampSerial = tstInfo.serialNumber.valueBlock.valueHexView.slice();
        //#endregion

        //#region Check that we do have detached data content
        if (data.byteLength === 0) {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 4,
            message: "Missed detached data input array",
          });
        }
        //#endregion

        if (!(await tstInfo.verify({ data }, crypto))) {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 15,
            message: "Error during verification: TSTInfo verification is failed",
            signatureVerified: false,
            signerCertificate: signerCert,
            timestampSerial,
            signerCertificateVerified: true
          });
        }
      }

      //#endregion

      if (checkChain) {
        const certs = this.certificates.filter(certificate => (certificate instanceof Certificate && !!checkCA(certificate, signerCert))) as Certificate[];
        const chainParams: CertificateChainValidationEngineParameters = {
          checkDate,
          certs,
          trustedCerts,
        };

        if (findIssuer) {
          chainParams.findIssuer = findIssuer;
        }
        if (findOrigin) {
          chainParams.findOrigin = findOrigin;
        }

        const chainEngine = new CertificateChainValidationEngine(chainParams);
        chainEngine.certs.push(signerCert);

        if (this.crls) {
          for (const crl of this.crls) {
            if ("thisUpdate" in crl)
              chainEngine.crls.push(crl);
            else // Assumed "revocation value" has "OtherRevocationInfoFormat"
            {
              if (crl.otherRevInfoFormat === id_PKIX_OCSP_Basic) // Basic OCSP response
                chainEngine.ocsps.push(new BasicOCSPResponse({ schema: crl.otherRevInfo }));
            }
          }
        }

        if (this.ocsps) {
          chainEngine.ocsps.push(...(this.ocsps));
        }

        const verificationResult = await chainEngine.verify({ passedWhenNotRevValues }, crypto)
          .catch(e => {
            throw new SignedDataVerifyError({
              date: checkDate,
              code: 5,
              message: `Validation of signer's certificate failed with error: ${((e instanceof Object) ? e.resultMessage : e)}`,
              signerCertificate: signerCert,
              signerCertificateVerified: false
            });
          });
        if (verificationResult.certificatePath) {
          certificatePath = verificationResult.certificatePath;
        }

        if (!verificationResult.result)
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 5,
            message: `Validation of signer's certificate failed: ${verificationResult.resultMessage}`,
            signerCertificate: signerCert,
            signerCertificateVerified: false
          });
      }
      //#endregion

      //#region Find signer's hashing algorithm

      const signerInfoHashAlgorithm = crypto.getAlgorithmByOID(signerInfo.digestAlgorithm.algorithmId);
      if (!("name" in signerInfoHashAlgorithm)) {
        throw new SignedDataVerifyError({
          date: checkDate,
          code: 7,
          message: `Unsupported signature algorithm: ${signerInfo.digestAlgorithm.algorithmId}`,
          signerCertificate: signerCert,
          signerCertificateVerified: true
        });
      }

      shaAlgorithm = signerInfoHashAlgorithm.name;
      //#endregion

      //#region Create correct data block for verification

      const eContent = this.encapContentInfo.eContent;
      if (eContent) // Attached data
      {
        if ((eContent.idBlock.tagClass === 1) &&
          (eContent.idBlock.tagNumber === 4)) {
          data = eContent.getValue();
        }
        else
          data = eContent.valueBlock.valueBeforeDecodeView;
      }
      else // Detached data
      {
        if (data.byteLength === 0) // Check that "data" already provided by function parameter
        {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 8,
            message: "Missed detached data input array",
            signerCertificate: signerCert,
            signerCertificateVerified: true
          });
        }
      }

      if (signerInfo.signedAttrs) {
        //#region Check mandatory attributes
        let foundContentType = false;
        let foundMessageDigest = false;

        for (const attribute of signerInfo.signedAttrs.attributes) {
          //#region Check that "content-type" attribute exists
          if (attribute.type === "1.2.840.113549.1.9.3")
            foundContentType = true;
          //#endregion

          //#region Check that "message-digest" attribute exists
          if (attribute.type === "1.2.840.113549.1.9.4") {
            foundMessageDigest = true;
            messageDigestValue = attribute.values[0].valueBlock.valueHex;
          }
          //#endregion

          //#region Speed-up searching
          if (foundContentType && foundMessageDigest)
            break;
          //#endregion
        }

        if (foundContentType === false) {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 9,
            message: "Attribute \"content-type\" is a mandatory attribute for \"signed attributes\"",
            signerCertificate: signerCert,
            signerCertificateVerified: true
          });
        }

        if (foundMessageDigest === false) {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 10,
            message: "Attribute \"message-digest\" is a mandatory attribute for \"signed attributes\"",
            signatureVerified: null,
            signerCertificate: signerCert,
            signerCertificateVerified: true
          });
        }
        //#endregion
      }
      //#endregion

      //#region Verify "message-digest" attribute in case of "signedAttrs"
      if (signerInfo.signedAttrs) {
        const messageDigest = await crypto.digest(shaAlgorithm, new Uint8Array(data));
        if (!pvutils.isEqualBuffer(messageDigest, messageDigestValue)) {
          throw new SignedDataVerifyError({
            date: checkDate,
            code: 15,
            message: "Error during verification: Message digest doesn't match",
            signatureVerified: null,
            signerCertificate: signerCert,
            timestampSerial,
            signerCertificateVerified: true
          });
        }
        data = signerInfo.signedAttrs.encodedValue;
      }
      //#endregion

      const verifyResult = await crypto.verifyWithPublicKey(data, signerInfo.signature, signerCert.subjectPublicKeyInfo, signerCert.signatureAlgorithm, shaAlgorithm);

      //#region Make a final result

      if (extendedMode) {
        return {
          date: checkDate,
          code: 14,
          message: EMPTY_STRING,
          signatureVerified: verifyResult,
          signerCertificate: signerCert,
          timestampSerial,
          signerCertificateVerified: true,
          certificatePath
        };
      } else {
        return verifyResult;
      }
    } catch (e) {
      if (e instanceof SignedDataVerifyError) {
        throw e;
      }
      throw new SignedDataVerifyError({
        date: checkDate,
        code: 15,
        message: `Error during verification: ${e instanceof Error ? e.message : e}`,
        signatureVerified: null,
        signerCertificate: signerCert,
        timestampSerial,
        signerCertificateVerified: true
      });
    }
  }

  /**
   * Signing current SignedData
   * @param privateKey Private key for "subjectPublicKeyInfo" structure
   * @param signerIndex Index number (starting from 0) of signer index to make signature for
   * @param hashAlgorithm Hashing algorithm. Default SHA-1
   * @param data Detached data
   * @param crypto Crypto engine
   */
  public async sign(privateKey: CryptoKey, signerIndex: number, hashAlgorithm = "SHA-1", data: BufferSource = (EMPTY_BUFFER), crypto = common.getCrypto(true)): Promise<void> {
    //#region Initial checking
    if (!privateKey)
      throw new Error("Need to provide a private key for signing");
    //#endregion

    //#region Simple check for supported algorithm
    const hashAlgorithmOID = crypto.getOIDByAlgorithm({ name: hashAlgorithm }, true, "hashAlgorithm");
    //#endregion

    //#region Append information about hash algorithm
    if ((this.digestAlgorithms.filter(algorithm => algorithm.algorithmId === hashAlgorithmOID)).length === 0) {
      this.digestAlgorithms.push(new AlgorithmIdentifier({
        algorithmId: hashAlgorithmOID,
        algorithmParams: new asn1js.Null()
      }));
    }

    const signerInfo = this.signerInfos[signerIndex];
    if (!signerInfo) {
      throw new RangeError("SignerInfo index is out of range");
    }
    signerInfo.digestAlgorithm = new AlgorithmIdentifier({
      algorithmId: hashAlgorithmOID,
      algorithmParams: new asn1js.Null()
    });
    //#endregion

    //#region Get a "default parameters" for current algorithm and set correct signature algorithm
    const signatureParams = await crypto.getSignatureParameters(privateKey, hashAlgorithm);
    const parameters = signatureParams.parameters;
    signerInfo.signatureAlgorithm = signatureParams.signatureAlgorithm;
    //#endregion

    //#region Create TBS data for signing
    if (signerInfo.signedAttrs) {
      if (signerInfo.signedAttrs.encodedValue.byteLength !== 0)
        data = signerInfo.signedAttrs.encodedValue;
      else {
        data = signerInfo.signedAttrs.toSchema().toBER();

        //#region Change type from "[0]" to "SET" accordingly to standard
        const view = pvtsutils.BufferSourceConverter.toUint8Array(data);
        view[0] = 0x31;
        //#endregion
      }
    }
    else {
      const eContent = this.encapContentInfo.eContent;
      if (eContent) // Attached data
      {
        if ((eContent.idBlock.tagClass === 1) &&
          (eContent.idBlock.tagNumber === 4)) {
          data = eContent.getValue();
        }
        else
          data = eContent.valueBlock.valueBeforeDecodeView;
      }
      else // Detached data
      {
        if (data.byteLength === 0) // Check that "data" already provided by function parameter
          throw new Error("Missed detached data input array");
      }
    }
    //#endregion

    //#region Signing TBS data on provided private key
    const signature = await crypto.signWithPrivateKey(data, privateKey, parameters as any);
    signerInfo.signature = new asn1js.OctetString({ valueHex: signature });
    //#endregion
  }

}


