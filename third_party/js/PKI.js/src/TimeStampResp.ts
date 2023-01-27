import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { PKIStatusInfo, PKIStatusInfoJson, PKIStatusInfoSchema } from "./PKIStatusInfo";
import { ContentInfo, ContentInfoJson, ContentInfoSchema } from "./ContentInfo";
import { SignedData } from "./SignedData";
import * as Schema from "./Schema";
import { id_ContentType_SignedData } from "./ObjectIdentifiers";
import { Certificate } from "./Certificate";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";
import * as common from "./common";

const STATUS = "status";
const TIME_STAMP_TOKEN = "timeStampToken";
const TIME_STAMP_RESP = "TimeStampResp";
const TIME_STAMP_RESP_STATUS = `${TIME_STAMP_RESP}.${STATUS}`;
const TIME_STAMP_RESP_TOKEN = `${TIME_STAMP_RESP}.${TIME_STAMP_TOKEN}`;
const CLEAR_PROPS = [
  TIME_STAMP_RESP_STATUS,
  TIME_STAMP_RESP_TOKEN
];

export interface ITimeStampResp {
  /**
   * Time-Stamp status
   */
  status: PKIStatusInfo;
  /**
   * Time-Stamp token
   */
  timeStampToken?: ContentInfo;
}

export interface TimeStampRespJson {
  status: PKIStatusInfoJson;
  timeStampToken?: ContentInfoJson;
}

export interface TimeStampRespVerifyParams {
  signer?: number;
  trustedCerts?: Certificate[];
  data?: ArrayBuffer;
}

export type TimeStampRespParameters = PkiObjectParameters & Partial<ITimeStampResp>;

/**
 * Represents the TimeStampResp structure described in [RFC3161](https://www.ietf.org/rfc/rfc3161.txt)
 *
 * @example The following example demonstrates how to create and sign Time-Stamp Response
 * ```js
 * // Generate random serial number
 * const serialNumber = pkijs.getRandomValues(new Uint8Array(10)).buffer;
 *
 * // Create specific TST info structure to sign
 * const tstInfo = new pkijs.TSTInfo({
 *   version: 1,
 *   policy: tspReq.reqPolicy,
 *   messageImprint: tspReq.messageImprint,
 *   serialNumber: new asn1js.Integer({ valueHex: serialNumber }),
 *   genTime: new Date(),
 *   ordering: true,
 *   accuracy: new pkijs.Accuracy({
 *     seconds: 1,
 *     millis: 1,
 *     micros: 10
 *   }),
 *   nonce: tspReq.nonce,
 * });
 *
 * // Create and sign CMS Signed Data with TSTInfo
 * const cmsSigned = new pkijs.SignedData({
 *   version: 3,
 *   encapContentInfo: new pkijs.EncapsulatedContentInfo({
 *     eContentType: "1.2.840.113549.1.9.16.1.4", // "tSTInfo" content type
 *     eContent: new asn1js.OctetString({ valueHex: tstInfo.toSchema().toBER() }),
 *   }),
 *   signerInfos: [
 *     new pkijs.SignerInfo({
 *       version: 1,
 *       sid: new pkijs.IssuerAndSerialNumber({
 *         issuer: cert.issuer,
 *         serialNumber: cert.serialNumber
 *       })
 *     })
 *   ],
 *   certificates: [cert]
 * });
 *
 * await cmsSigned.sign(keys.privateKey, 0, "SHA-256");
 *
 * // Create CMS Content Info
 * const cmsContent = new pkijs.ContentInfo({
 *   contentType: pkijs.ContentInfo.SIGNED_DATA,
 *   content: cmsSigned.toSchema(true)
 * });
 *
 * // Finally create completed TSP response structure
 * const tspResp = new pkijs.TimeStampResp({
 *   status: new pkijs.PKIStatusInfo({ status: pkijs.PKIStatus.granted }),
 *   timeStampToken: new pkijs.ContentInfo({ schema: cmsContent.toSchema() })
 * });
 *
 * const tspRespRaw = tspResp.toSchema().toBER();
 * ```
 */
export class TimeStampResp extends PkiObject implements ITimeStampResp {

  public static override CLASS_NAME = "TimeStampResp";

  public status!: PKIStatusInfo;
  public timeStampToken?: ContentInfo;

  /**
   * Initializes a new instance of the {@link TimeStampResp} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: TimeStampRespParameters = {}) {
    super();

    this.status = pvutils.getParametersValue(parameters, STATUS, TimeStampResp.defaultValues(STATUS));
    if (TIME_STAMP_TOKEN in parameters) {
      this.timeStampToken = pvutils.getParametersValue(parameters, TIME_STAMP_TOKEN, TimeStampResp.defaultValues(TIME_STAMP_TOKEN));
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
  public static override defaultValues(memberName: typeof STATUS): PKIStatusInfo;
  public static override defaultValues(memberName: typeof TIME_STAMP_TOKEN): ContentInfo;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case STATUS:
        return new PKIStatusInfo();
      case TIME_STAMP_TOKEN:
        return new ContentInfo();
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
      case STATUS:
        return ((PKIStatusInfo.compareWithDefault(STATUS, memberValue.status)) &&
          (("statusStrings" in memberValue) === false) &&
          (("failInfo" in memberValue) === false));
      case TIME_STAMP_TOKEN:
        return ((memberValue.contentType === EMPTY_STRING) &&
          (memberValue.content instanceof asn1js.Any));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * TimeStampResp ::= SEQUENCE  {
   *    status                  PKIStatusInfo,
   *    timeStampToken          TimeStampToken     OPTIONAL  }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    status?: PKIStatusInfoSchema,
    timeStampToken?: ContentInfoSchema,
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || TIME_STAMP_RESP),
      value: [
        PKIStatusInfo.schema(names.status || {
          names: {
            blockName: TIME_STAMP_RESP_STATUS
          }
        }),
        ContentInfo.schema(names.timeStampToken || {
          names: {
            blockName: TIME_STAMP_RESP_TOKEN,
            optional: true
          }
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
      TimeStampResp.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.status = new PKIStatusInfo({ schema: asn1.result[TIME_STAMP_RESP_STATUS] });
    if (TIME_STAMP_RESP_TOKEN in asn1.result)
      this.timeStampToken = new ContentInfo({ schema: asn1.result[TIME_STAMP_RESP_TOKEN] });
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.status.toSchema());
    if (this.timeStampToken) {
      outputArray.push(this.timeStampToken.toSchema());
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): TimeStampRespJson {
    const res: TimeStampRespJson = {
      status: this.status.toJSON()
    };

    if (this.timeStampToken) {
      res.timeStampToken = this.timeStampToken.toJSON();
    }

    return res;
  }

  /**
   * Sign current TSP Response
   * @param privateKey Private key for "subjectPublicKeyInfo" structure
   * @param hashAlgorithm Hashing algorithm. Default SHA-1
   * @param crypto Crypto engine
   */
  public async sign(privateKey: CryptoKey, hashAlgorithm?: string, crypto = common.getCrypto(true)) {
    this.assertContentType();

    // Sign internal signed data value
    const signed = new SignedData({ schema: this.timeStampToken.content });

    return signed.sign(privateKey, 0, hashAlgorithm, undefined, crypto);
  }

  /**
   * Verify current TSP Response
   * @param verificationParameters Input parameters for verification
   * @param crypto Crypto engine
   */
  public async verify(verificationParameters: TimeStampRespVerifyParams = { signer: 0, trustedCerts: [], data: EMPTY_BUFFER }, crypto = common.getCrypto(true)): Promise<boolean> {
    this.assertContentType();

    // Verify internal signed data value
    const signed = new SignedData({ schema: this.timeStampToken.content });

    return signed.verify(verificationParameters, crypto);
  }

  private assertContentType(): asserts this is { timeStampToken: ContentInfo; } {
    if (!this.timeStampToken) {
      throw new Error("timeStampToken is absent in TSP response");
    }
    if (this.timeStampToken.contentType !== id_ContentType_SignedData) { // Must be a CMS signed data
      throw new Error(`Wrong format of timeStampToken: ${this.timeStampToken.contentType}`);
    }
  }
}

