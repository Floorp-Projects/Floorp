import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as common from "./common";
import { ResponseData, ResponseDataJson, ResponseDataSchema } from "./ResponseData";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { Certificate, CertificateJson, CertificateSchema, checkCA } from "./Certificate";
import { CertID } from "./CertID";
import { RelativeDistinguishedNames } from "./RelativeDistinguishedNames";
import { CertificateChainValidationEngine } from "./CertificateChainValidationEngine";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const TBS_RESPONSE_DATA = "tbsResponseData";
const SIGNATURE_ALGORITHM = "signatureAlgorithm";
const SIGNATURE = "signature";
const CERTS = "certs";
const BASIC_OCSP_RESPONSE = "BasicOCSPResponse";
const BASIC_OCSP_RESPONSE_TBS_RESPONSE_DATA = `${BASIC_OCSP_RESPONSE}.${TBS_RESPONSE_DATA}`;
const BASIC_OCSP_RESPONSE_SIGNATURE_ALGORITHM = `${BASIC_OCSP_RESPONSE}.${SIGNATURE_ALGORITHM}`;
const BASIC_OCSP_RESPONSE_SIGNATURE = `${BASIC_OCSP_RESPONSE}.${SIGNATURE}`;
const BASIC_OCSP_RESPONSE_CERTS = `${BASIC_OCSP_RESPONSE}.${CERTS}`;
const CLEAR_PROPS = [
  BASIC_OCSP_RESPONSE_TBS_RESPONSE_DATA,
  BASIC_OCSP_RESPONSE_SIGNATURE_ALGORITHM,
  BASIC_OCSP_RESPONSE_SIGNATURE,
  BASIC_OCSP_RESPONSE_CERTS
];

export interface IBasicOCSPResponse {
  tbsResponseData: ResponseData;
  signatureAlgorithm: AlgorithmIdentifier;
  signature: asn1js.BitString;
  certs?: Certificate[];
}

export interface CertificateStatus {
  isForCertificate: boolean;
  /**
   * 0 = good, 1 = revoked, 2 = unknown
   */
  status: number;
}

export type BasicOCSPResponseParameters = PkiObjectParameters & Partial<IBasicOCSPResponse>;

export interface BasicOCSPResponseVerifyParams {
  trustedCerts?: Certificate[];
}

export interface BasicOCSPResponseJson {
  tbsResponseData: ResponseDataJson;
  signatureAlgorithm: AlgorithmIdentifierJson;
  signature: asn1js.BitStringJson;
  certs?: CertificateJson[];
}

/**
 * Represents the BasicOCSPResponse structure described in [RFC6960](https://datatracker.ietf.org/doc/html/rfc6960)
 */
export class BasicOCSPResponse extends PkiObject implements IBasicOCSPResponse {

  public static override CLASS_NAME = "BasicOCSPResponse";

  public tbsResponseData!: ResponseData;
  public signatureAlgorithm!: AlgorithmIdentifier;
  public signature!: asn1js.BitString;
  public certs?: Certificate[];

  /**
   * Initializes a new instance of the {@link BasicOCSPResponse} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: BasicOCSPResponseParameters = {}) {
    super();

    this.tbsResponseData = pvutils.getParametersValue(parameters, TBS_RESPONSE_DATA, BasicOCSPResponse.defaultValues(TBS_RESPONSE_DATA));
    this.signatureAlgorithm = pvutils.getParametersValue(parameters, SIGNATURE_ALGORITHM, BasicOCSPResponse.defaultValues(SIGNATURE_ALGORITHM));
    this.signature = pvutils.getParametersValue(parameters, SIGNATURE, BasicOCSPResponse.defaultValues(SIGNATURE));
    if (CERTS in parameters) {
      this.certs = pvutils.getParametersValue(parameters, CERTS, BasicOCSPResponse.defaultValues(CERTS));
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
  public static override defaultValues(memberName: typeof TBS_RESPONSE_DATA): ResponseData;
  public static override defaultValues(memberName: typeof SIGNATURE_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SIGNATURE): asn1js.BitString;
  public static override defaultValues(memberName: typeof CERTS): Certificate[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TBS_RESPONSE_DATA:
        return new ResponseData();
      case SIGNATURE_ALGORITHM:
        return new AlgorithmIdentifier();
      case SIGNATURE:
        return new asn1js.BitString();
      case CERTS:
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
      case "type":
        {
          let comparisonResult = ((ResponseData.compareWithDefault("tbs", memberValue.tbs)) &&
            (ResponseData.compareWithDefault("responderID", memberValue.responderID)) &&
            (ResponseData.compareWithDefault("producedAt", memberValue.producedAt)) &&
            (ResponseData.compareWithDefault("responses", memberValue.responses)));

          if ("responseExtensions" in memberValue)
            comparisonResult = comparisonResult && (ResponseData.compareWithDefault("responseExtensions", memberValue.responseExtensions));

          return comparisonResult;
        }
      case SIGNATURE_ALGORITHM:
        return ((memberValue.algorithmId === EMPTY_STRING) && (("algorithmParams" in memberValue) === false));
      case SIGNATURE:
        return (memberValue.isEqual(BasicOCSPResponse.defaultValues(memberName)));
      case CERTS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * BasicOCSPResponse ::= SEQUENCE {
   *    tbsResponseData      ResponseData,
   *    signatureAlgorithm   AlgorithmIdentifier,
   *    signature            BIT STRING,
   *    certs            [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    tbsResponseData?: ResponseDataSchema;
    signatureAlgorithm?: AlgorithmIdentifierSchema;
    signature?: string;
    certs?: CertificateSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || BASIC_OCSP_RESPONSE),
      value: [
        ResponseData.schema(names.tbsResponseData || {
          names: {
            blockName: BASIC_OCSP_RESPONSE_TBS_RESPONSE_DATA
          }
        }),
        AlgorithmIdentifier.schema(names.signatureAlgorithm || {
          names: {
            blockName: BASIC_OCSP_RESPONSE_SIGNATURE_ALGORITHM
          }
        }),
        new asn1js.BitString({ name: (names.signature || BASIC_OCSP_RESPONSE_SIGNATURE) }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            new asn1js.Sequence({
              value: [new asn1js.Repeated({
                name: BASIC_OCSP_RESPONSE_CERTS,
                value: Certificate.schema(names.certs || {})
              })]
            })
          ]
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);
    //#endregion

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      BasicOCSPResponse.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.tbsResponseData = new ResponseData({ schema: asn1.result[BASIC_OCSP_RESPONSE_TBS_RESPONSE_DATA] });
    this.signatureAlgorithm = new AlgorithmIdentifier({ schema: asn1.result[BASIC_OCSP_RESPONSE_SIGNATURE_ALGORITHM] });
    this.signature = asn1.result[BASIC_OCSP_RESPONSE_SIGNATURE];

    if (BASIC_OCSP_RESPONSE_CERTS in asn1.result) {
      this.certs = Array.from(asn1.result[BASIC_OCSP_RESPONSE_CERTS], element => new Certificate({ schema: element }));
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.tbsResponseData.toSchema());
    outputArray.push(this.signatureAlgorithm.toSchema());
    outputArray.push(this.signature);

    //#region Create array of certificates
    if (this.certs) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          new asn1js.Sequence({
            value: Array.from(this.certs, o => o.toSchema())
          })
        ]
      }));
    }
    //#endregion
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): BasicOCSPResponseJson {
    const res: BasicOCSPResponseJson = {
      tbsResponseData: this.tbsResponseData.toJSON(),
      signatureAlgorithm: this.signatureAlgorithm.toJSON(),
      signature: this.signature.toJSON(),
    };

    if (this.certs) {
      res.certs = Array.from(this.certs, o => o.toJSON());
    }

    return res;
  }

  /**
   * Get OCSP response status for specific certificate
   * @param certificate Certificate to be checked
   * @param issuerCertificate Certificate of issuer for certificate to be checked
   * @param crypto Crypto engine
   */
  public async getCertificateStatus(certificate: Certificate, issuerCertificate: Certificate, crypto = common.getCrypto(true)): Promise<CertificateStatus> {
    //#region Initial variables
    const result = {
      isForCertificate: false,
      status: 2 // 0 = good, 1 = revoked, 2 = unknown
    };

    const hashesObject: Record<string, number> = {};

    const certIDs: CertID[] = [];
    //#endregion

    //#region Create all "certIDs" for input certificates
    for (const response of this.tbsResponseData.responses) {
      const hashAlgorithm = crypto.getAlgorithmByOID(response.certID.hashAlgorithm.algorithmId, true, "CertID.hashAlgorithm");

      if (!hashesObject[hashAlgorithm.name]) {
        hashesObject[hashAlgorithm.name] = 1;

        const certID = new CertID();

        certIDs.push(certID);
        await certID.createForCertificate(certificate, {
          hashAlgorithm: hashAlgorithm.name,
          issuerCertificate
        }, crypto);
      }
    }
    //#endregion

    //#region Compare all response's "certIDs" with identifiers for input certificate
    for (const response of this.tbsResponseData.responses) {
      for (const id of certIDs) {
        if (response.certID.isEqual(id)) {
          result.isForCertificate = true;

          try {
            switch (response.certStatus.idBlock.isConstructed) {
              case true:
                if (response.certStatus.idBlock.tagNumber === 1)
                  result.status = 1; // revoked

                break;
              case false:
                switch (response.certStatus.idBlock.tagNumber) {
                  case 0: // good
                    result.status = 0;
                    break;
                  case 2: // unknown
                    result.status = 2;
                    break;
                  default:
                }

                break;
              default:
            }
          }
          catch (ex) {
            // nothing
          }

          return result;
        }
      }
    }

    return result;
    //#endregion
  }

  /**
   * Make signature for current OCSP Basic Response
   * @param privateKey Private key for "subjectPublicKeyInfo" structure
   * @param hashAlgorithm Hashing algorithm. Default SHA-1
   * @param crypto Crypto engine
   */
  async sign(privateKey: CryptoKey, hashAlgorithm = "SHA-1", crypto = common.getCrypto(true)): Promise<void> {
    // Get a private key from function parameter
    if (!privateKey) {
      throw new Error("Need to provide a private key for signing");
    }

    //#region Get a "default parameters" for current algorithm and set correct signature algorithm
    const signatureParams = await crypto.getSignatureParameters(privateKey, hashAlgorithm);

    const algorithm = signatureParams.parameters.algorithm;
    if (!("name" in algorithm)) {
      throw new Error("Empty algorithm");
    }
    this.signatureAlgorithm = signatureParams.signatureAlgorithm;
    //#endregion

    //#region Create TBS data for signing
    this.tbsResponseData.tbsView = new Uint8Array(this.tbsResponseData.toSchema(true).toBER());
    //#endregion

    //#region Signing TBS data on provided private key
    const signature = await crypto.signWithPrivateKey(this.tbsResponseData.tbsView, privateKey, { algorithm });
    this.signature = new asn1js.BitString({ valueHex: signature });
    //#endregion
  }

  /**
   * Verify existing OCSP Basic Response
   * @param params Additional parameters
   * @param crypto Crypto engine
   */
  public async verify(params: BasicOCSPResponseVerifyParams = {}, crypto = common.getCrypto(true)): Promise<boolean> {
    //#region Initial variables
    let signerCert: Certificate | null = null;
    let certIndex = -1;
    const trustedCerts: Certificate[] = params.trustedCerts || [];
    //#endregion

    //#region Check amount of certificates
    if (!this.certs) {
      throw new Error("No certificates attached to the BasicOCSPResponse");
    }
    //#endregion

    //#region Find correct value for "responderID"
    switch (true) {
      case (this.tbsResponseData.responderID instanceof RelativeDistinguishedNames): // [1] Name
        for (const [index, certificate] of this.certs.entries()) {
          if (certificate.subject.isEqual(this.tbsResponseData.responderID)) {
            certIndex = index;
            break;
          }
        }
        break;
      case (this.tbsResponseData.responderID instanceof asn1js.OctetString): // [2] KeyHash
        for (const [index, cert] of this.certs.entries()) {
          const hash = await crypto.digest({ name: "sha-1" }, cert.subjectPublicKeyInfo.subjectPublicKey.valueBlock.valueHexView);
          if (pvutils.isEqualBuffer(hash, this.tbsResponseData.responderID.valueBlock.valueHex)) {
            certIndex = index;
            break;
          }
        }
        break;
      default:
        throw new Error("Wrong value for responderID");
    }
    //#endregion

    //#region Make additional verification for signer's certificate
    if (certIndex === (-1))
      throw new Error("Correct certificate was not found in OCSP response");

    signerCert = this.certs[certIndex];

    const additionalCerts: Certificate[] = [signerCert];
    for (const cert of this.certs) {
      const caCert = await checkCA(cert, signerCert);
      if (caCert) {
        additionalCerts.push(caCert);
      }
    }
    const certChain = new CertificateChainValidationEngine({
      certs: additionalCerts,
      trustedCerts,
    });

    const verificationResult = await certChain.verify({}, crypto);
    if (!verificationResult.result) {
      throw new Error("Validation of signer's certificate failed");
    }

    return crypto.verifyWithPublicKey(this.tbsResponseData.tbsView, this.signature, this.certs[certIndex].subjectPublicKeyInfo, this.signatureAlgorithm);
  }

}
