import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as common from "./common";
import { TBSRequest, TBSRequestJson, TBSRequestSchema } from "./TBSRequest";
import { Signature, SignatureJson, SignatureSchema } from "./Signature";
import { Request } from "./Request";
import { CertID, CertIDCreateParams } from "./CertID";
import * as Schema from "./Schema";
import { Certificate } from "./Certificate";
import { AsnError, ParameterError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";

const TBS_REQUEST = "tbsRequest";
const OPTIONAL_SIGNATURE = "optionalSignature";
const CLEAR_PROPS = [
  TBS_REQUEST,
  OPTIONAL_SIGNATURE
];

export interface IOCSPRequest {
  tbsRequest: TBSRequest;
  optionalSignature?: Signature;
}

export interface OCSPRequestJson {
  tbsRequest: TBSRequestJson;
  optionalSignature?: SignatureJson;
}

export type OCSPRequestParameters = PkiObjectParameters & Partial<IOCSPRequest>;

/**
 * Represents an OCSP request described in [RFC6960 Section 4.1](https://datatracker.ietf.org/doc/html/rfc6960#section-4.1)
 *
 * @example The following example demonstrates how to create OCSP request
 * ```js
 * // Create OCSP request
 * const ocspReq = new pkijs.OCSPRequest();
 *
 * ocspReq.tbsRequest.requestorName = new pkijs.GeneralName({
 *   type: 4,
 *   value: cert.subject,
 * });
 *
 * await ocspReq.createForCertificate(cert, {
 *   hashAlgorithm: "SHA-256",
 *   issuerCertificate: issuerCert,
 * });
 *
 * const nonce = pkijs.getRandomValues(new Uint8Array(10));
 * ocspReq.tbsRequest.requestExtensions = [
 *   new pkijs.Extension({
 *     extnID: "1.3.6.1.5.5.7.48.1.2", // nonce
 *     extnValue: new asn1js.OctetString({ valueHex: nonce.buffer }).toBER(),
 *   })
 * ];
 *
 * // Encode OCSP request
 * const ocspReqRaw = ocspReq.toSchema(true).toBER();
 * ```
 */
export class OCSPRequest extends PkiObject implements IOCSPRequest {

  public static override CLASS_NAME = "OCSPRequest";

  public tbsRequest!: TBSRequest;
  public optionalSignature?: Signature;

  /**
   * Initializes a new instance of the {@link OCSPRequest} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OCSPRequestParameters = {}) {
    super();

    this.tbsRequest = pvutils.getParametersValue(parameters, TBS_REQUEST, OCSPRequest.defaultValues(TBS_REQUEST));
    if (OPTIONAL_SIGNATURE in parameters) {
      this.optionalSignature = pvutils.getParametersValue(parameters, OPTIONAL_SIGNATURE, OCSPRequest.defaultValues(OPTIONAL_SIGNATURE));
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
  public static override defaultValues(memberName: typeof TBS_REQUEST): TBSRequest;
  public static override defaultValues(memberName: typeof OPTIONAL_SIGNATURE): Signature;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TBS_REQUEST:
        return new TBSRequest();
      case OPTIONAL_SIGNATURE:
        return new Signature();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compare values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   * @returns Returns `true` if `memberValue` is equal to default value for selected class member
   */
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case TBS_REQUEST:
        return ((TBSRequest.compareWithDefault("tbs", memberValue.tbs)) &&
          (TBSRequest.compareWithDefault("version", memberValue.version)) &&
          (TBSRequest.compareWithDefault("requestorName", memberValue.requestorName)) &&
          (TBSRequest.compareWithDefault("requestList", memberValue.requestList)) &&
          (TBSRequest.compareWithDefault("requestExtensions", memberValue.requestExtensions)));
      case OPTIONAL_SIGNATURE:
        return ((Signature.compareWithDefault("signatureAlgorithm", memberValue.signatureAlgorithm)) &&
          (Signature.compareWithDefault("signature", memberValue.signature)) &&
          (Signature.compareWithDefault("certs", memberValue.certs)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * OCSPRequest ::= SEQUENCE {
   *    tbsRequest                  TBSRequest,
   *    optionalSignature   [0]     EXPLICIT Signature OPTIONAL }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    tbsRequest?: TBSRequestSchema;
    optionalSignature?: SignatureSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: names.blockName || "OCSPRequest",
      value: [
        TBSRequest.schema(names.tbsRequest || {
          names: {
            blockName: TBS_REQUEST
          }
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            Signature.schema(names.optionalSignature || {
              names: {
                blockName: OPTIONAL_SIGNATURE
              }
            })
          ]
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      OCSPRequest.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.tbsRequest = new TBSRequest({ schema: asn1.result.tbsRequest });
    if (OPTIONAL_SIGNATURE in asn1.result)
      this.optionalSignature = new Signature({ schema: asn1.result.optionalSignature });
  }

  public toSchema(encodeFlag = false) {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.tbsRequest.toSchema(encodeFlag));
    if (this.optionalSignature)
      outputArray.push(
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            this.optionalSignature.toSchema()
          ]
        }));
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): OCSPRequestJson {
    const res: OCSPRequestJson = {
      tbsRequest: this.tbsRequest.toJSON()
    };

    if (this.optionalSignature) {
      res.optionalSignature = this.optionalSignature.toJSON();
    }

    return res;
  }

  /**
   * Making OCSP Request for specific certificate
   * @param certificate Certificate making OCSP Request for
   * @param parameters Additional parameters
   * @param crypto Crypto engine
   */
  public async createForCertificate(certificate: Certificate, parameters: CertIDCreateParams, crypto = common.getCrypto(true)): Promise<void> {
    //#region Initial variables
    const certID = new CertID();
    //#endregion

    //#region Create OCSP certificate identifier for the certificate
    await certID.createForCertificate(certificate, parameters, crypto);
    //#endregion

    //#region Make final request data
    this.tbsRequest.requestList.push(new Request({
      reqCert: certID,
    }));
    //#endregion
  }

  /**
   * Make signature for current OCSP Request
   * @param privateKey Private key for "subjectPublicKeyInfo" structure
   * @param hashAlgorithm Hashing algorithm. Default SHA-1
   * @param crypto Crypto engine
   */
  public async sign(privateKey: CryptoKey, hashAlgorithm = "SHA-1", crypto = common.getCrypto(true)) {
    // Initial checking
    ParameterError.assertEmpty(privateKey, "privateKey", "OCSPRequest.sign method");

    // Check that OPTIONAL_SIGNATURE exists in the current request
    if (!this.optionalSignature) {
      throw new Error("Need to create \"optionalSignature\" field before signing");
    }

    //#region Get a "default parameters" for current algorithm and set correct signature algorithm
    const signatureParams = await crypto.getSignatureParameters(privateKey, hashAlgorithm);
    const parameters = signatureParams.parameters;
    this.optionalSignature.signatureAlgorithm = signatureParams.signatureAlgorithm;
    //#endregion

    //#region Create TBS data for signing
    const tbs = this.tbsRequest.toSchema(true).toBER(false);
    //#endregion

    // Signing TBS data on provided private key
    const signature = await crypto.signWithPrivateKey(tbs, privateKey, parameters as any);
    this.optionalSignature.signature = new asn1js.BitString({ valueHex: signature });
  }

  verify() {
    // TODO: Create the function
  }

}
