import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { ResponseBytes, ResponseBytesJson, ResponseBytesSchema } from "./ResponseBytes";
import { BasicOCSPResponse } from "./BasicOCSPResponse";
import * as Schema from "./Schema";
import { Certificate } from "./Certificate";
import { id_PKIX_OCSP_Basic } from "./ObjectIdentifiers";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as common from "./common";

const RESPONSE_STATUS = "responseStatus";
const RESPONSE_BYTES = "responseBytes";

export interface IOCSPResponse {
  responseStatus: asn1js.Enumerated;
  responseBytes?: ResponseBytes;
}

export interface OCSPResponseJson {
  responseStatus: asn1js.EnumeratedJson;
  responseBytes?: ResponseBytesJson;
}

export type OCSPResponseParameters = PkiObjectParameters & Partial<IOCSPResponse>;

/**
 * Represents an OCSP response described in [RFC6960 Section 4.2](https://datatracker.ietf.org/doc/html/rfc6960#section-4.2)
 *
 * @example The following example demonstrates how to verify OCSP response
 * ```js
 * const asnOcspResp = asn1js.fromBER(ocspRespRaw);
 * const ocspResp = new pkijs.OCSPResponse({ schema: asnOcspResp.result });
 *
 * if (!ocspResp.responseBytes) {
 *   throw new Error("No \"ResponseBytes\" in the OCSP Response - nothing to verify");
 * }
 *
 * const asnOcspRespBasic = asn1js.fromBER(ocspResp.responseBytes.response.valueBlock.valueHex);
 * const ocspBasicResp = new pkijs.BasicOCSPResponse({ schema: asnOcspRespBasic.result });
 * const ok = await ocspBasicResp.verify({ trustedCerts: [cert] });
 * ```
 *
 * @example The following example demonstrates how to create OCSP response
 * ```js
 * const ocspBasicResp = new pkijs.BasicOCSPResponse();
 *
 * // Create specific TST info structure to sign
 * ocspBasicResp.tbsResponseData.responderID = issuerCert.subject;
 * ocspBasicResp.tbsResponseData.producedAt = new Date();
 *
 * const certID = new pkijs.CertID();
 * await certID.createForCertificate(cert, {
 *   hashAlgorithm: "SHA-256",
 *   issuerCertificate: issuerCert,
 * });
 * const response = new pkijs.SingleResponse({
 *   certID,
 * });
 * response.certStatus = new asn1js.Primitive({
 *   idBlock: {
 *     tagClass: 3, // CONTEXT-SPECIFIC
 *     tagNumber: 0 // [0]
 *   },
 *   lenBlockLength: 1 // The length contains one byte 0x00
 * }); // status - success
 * response.thisUpdate = new Date();
 *
 * ocspBasicResp.tbsResponseData.responses.push(response);
 *
 * // Add certificates for chain OCSP response validation
 * ocspBasicResp.certs = [issuerCert];
 *
 * await ocspBasicResp.sign(keys.privateKey, "SHA-256");
 *
 * // Finally create completed OCSP response structure
 * const ocspBasicRespRaw = ocspBasicResp.toSchema().toBER(false);
 *
 * const ocspResp = new pkijs.OCSPResponse({
 *   responseStatus: new asn1js.Enumerated({ value: 0 }), // success
 *   responseBytes: new pkijs.ResponseBytes({
 *     responseType: pkijs.id_PKIX_OCSP_Basic,
 *     response: new asn1js.OctetString({ valueHex: ocspBasicRespRaw }),
 *   }),
 * });
 *
 * const ocspRespRaw = ocspResp.toSchema().toBER();
 * ```
 */
export class OCSPResponse extends PkiObject implements IOCSPResponse {

  public static override CLASS_NAME = "OCSPResponse";

  public responseStatus!: asn1js.Enumerated;
  public responseBytes?: ResponseBytes;

  /**
   * Initializes a new instance of the {@link OCSPResponse} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OCSPResponseParameters = {}) {
    super();

    this.responseStatus = pvutils.getParametersValue(parameters, RESPONSE_STATUS, OCSPResponse.defaultValues(RESPONSE_STATUS));
    if (RESPONSE_BYTES in parameters) {
      this.responseBytes = pvutils.getParametersValue(parameters, RESPONSE_BYTES, OCSPResponse.defaultValues(RESPONSE_BYTES));
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
  public static override defaultValues(memberName: typeof RESPONSE_STATUS): asn1js.Enumerated;
  public static override defaultValues(memberName: typeof RESPONSE_BYTES): ResponseBytes;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case RESPONSE_STATUS:
        return new asn1js.Enumerated();
      case RESPONSE_BYTES:
        return new ResponseBytes();
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
      case RESPONSE_STATUS:
        return (memberValue.isEqual(OCSPResponse.defaultValues(memberName)));
      case RESPONSE_BYTES:
        return ((ResponseBytes.compareWithDefault("responseType", memberValue.responseType)) &&
          (ResponseBytes.compareWithDefault("response", memberValue.response)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * OCSPResponse ::= SEQUENCE {
   *    responseStatus         OCSPResponseStatus,
   *    responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }
   *
   * OCSPResponseStatus ::= ENUMERATED {
   *    successful            (0),  -- Response has valid confirmations
   *    malformedRequest      (1),  -- Illegal confirmation request
   *    internalError         (2),  -- Internal error in issuer
   *    tryLater              (3),  -- Try again later
   *    -- (4) is not used
   *    sigRequired           (5),  -- Must sign the request
   *    unauthorized          (6)   -- Request unauthorized
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    responseStatus?: string;
    responseBytes?: ResponseBytesSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || "OCSPResponse"),
      value: [
        new asn1js.Enumerated({ name: (names.responseStatus || RESPONSE_STATUS) }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            ResponseBytes.schema(names.responseBytes || {
              names: {
                blockName: RESPONSE_BYTES
              }
            })
          ]
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, [
      RESPONSE_STATUS,
      RESPONSE_BYTES
    ]);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      OCSPResponse.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.responseStatus = asn1.result.responseStatus;
    if (RESPONSE_BYTES in asn1.result)
      this.responseBytes = new ResponseBytes({ schema: asn1.result.responseBytes });
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.responseStatus);
    if (this.responseBytes) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [this.responseBytes.toSchema()]
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): OCSPResponseJson {
    const res: OCSPResponseJson = {
      responseStatus: this.responseStatus.toJSON()
    };

    if (this.responseBytes) {
      res.responseBytes = this.responseBytes.toJSON();
    }

    return res;
  }

  /**
   * Get OCSP response status for specific certificate
   * @param certificate
   * @param issuerCertificate
   * @param crypto Crypto engine
   */
  public async getCertificateStatus(certificate: Certificate, issuerCertificate: Certificate, crypto = common.getCrypto(true)) {
    //#region Initial variables
    let basicResponse;

    const result = {
      isForCertificate: false,
      status: 2 // 0 = good, 1 = revoked, 2 = unknown
    };
    //#endregion

    //#region Check that RESPONSE_BYTES contain "OCSP_BASIC_RESPONSE"
    if (!this.responseBytes)
      return result;

    if (this.responseBytes.responseType !== id_PKIX_OCSP_Basic) // id-pkix-ocsp-basic
      return result;

    try {
      const asn1Basic = asn1js.fromBER(this.responseBytes.response.valueBlock.valueHexView);
      AsnError.assert(asn1Basic, "Basic OCSP response");
      basicResponse = new BasicOCSPResponse({ schema: asn1Basic.result });
    }
    catch (ex) {
      return result;
    }
    //#endregion

    return basicResponse.getCertificateStatus(certificate, issuerCertificate, crypto);
  }

  /**
   * Make a signature for current OCSP Response
   * @param privateKey Private key for "subjectPublicKeyInfo" structure
   * @param hashAlgorithm Hashing algorithm. Default SHA-1
   */
  public async sign(privateKey: CryptoKey, hashAlgorithm?: string, crypto = common.getCrypto(true)) {
    //#region Check that ResponseData has type BasicOCSPResponse and sign it
    if (this.responseBytes && this.responseBytes.responseType === id_PKIX_OCSP_Basic) {
      const basicResponse = BasicOCSPResponse.fromBER(this.responseBytes.response.valueBlock.valueHexView);

      return basicResponse.sign(privateKey, hashAlgorithm, crypto);
    }

    throw new Error(`Unknown ResponseBytes type: ${this.responseBytes?.responseType || "Unknown"}`);
    //#endregion
  }

  /**
   * Verify current OCSP Response
   * @param issuerCertificate In order to decrease size of resp issuer cert could be omitted. In such case you need manually provide it.
   * @param crypto Crypto engine
   */
  public async verify(issuerCertificate: Certificate | null = null, crypto = common.getCrypto(true)): Promise<boolean> {
    //#region Check that ResponseBytes exists in the object
    if ((RESPONSE_BYTES in this) === false)
      throw new Error("Empty ResponseBytes field");
    //#endregion

    //#region Check that ResponseData has type BasicOCSPResponse and verify it
    if (this.responseBytes && this.responseBytes.responseType === id_PKIX_OCSP_Basic) {
      const basicResponse = BasicOCSPResponse.fromBER(this.responseBytes.response.valueBlock.valueHexView);

      if (issuerCertificate !== null) {
        if (!basicResponse.certs) {
          basicResponse.certs = [];
        }

        basicResponse.certs.push(issuerCertificate);
      }

      return basicResponse.verify({}, crypto);
    }

    throw new Error(`Unknown ResponseBytes type: ${this.responseBytes?.responseType || "Unknown"}`);
    //#endregion
  }

}

