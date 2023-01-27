import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { MessageImprint, MessageImprintJson, MessageImprintSchema } from "./MessageImprint";
import { Extension, ExtensionJson } from "./Extension";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const VERSION = "version";
const MESSAGE_IMPRINT = "messageImprint";
const REQ_POLICY = "reqPolicy";
const NONCE = "nonce";
const CERT_REQ = "certReq";
const EXTENSIONS = "extensions";
const TIME_STAMP_REQ = "TimeStampReq";
const TIME_STAMP_REQ_VERSION = `${TIME_STAMP_REQ}.${VERSION}`;
const TIME_STAMP_REQ_MESSAGE_IMPRINT = `${TIME_STAMP_REQ}.${MESSAGE_IMPRINT}`;
const TIME_STAMP_REQ_POLICY = `${TIME_STAMP_REQ}.${REQ_POLICY}`;
const TIME_STAMP_REQ_NONCE = `${TIME_STAMP_REQ}.${NONCE}`;
const TIME_STAMP_REQ_CERT_REQ = `${TIME_STAMP_REQ}.${CERT_REQ}`;
const TIME_STAMP_REQ_EXTENSIONS = `${TIME_STAMP_REQ}.${EXTENSIONS}`;
const CLEAR_PROPS = [
  TIME_STAMP_REQ_VERSION,
  TIME_STAMP_REQ_MESSAGE_IMPRINT,
  TIME_STAMP_REQ_POLICY,
  TIME_STAMP_REQ_NONCE,
  TIME_STAMP_REQ_CERT_REQ,
  TIME_STAMP_REQ_EXTENSIONS,
];

export interface ITimeStampReq {
  /**
   * Version of the Time-Stamp request. Should be version 1.
   */
  version: number;
  /**
   * Contains the hash of the datum to be time-stamped
   */
  messageImprint: MessageImprint;
  /**
   * Indicates the TSA policy under which the TimeStampToken SHOULD be provided.
   */
  reqPolicy?: string;
  /**
   * The nonce, if included, allows the client to verify the timeliness of
   * the response when no local clock is available. The nonce is a large
   * random number with a high probability that the client generates it
   * only once.
   */
  nonce?: asn1js.Integer;
  /**
   * If the certReq field is present and set to true, the TSA's public key
   * certificate that is referenced by the ESSCertID identifier inside a
   * SigningCertificate attribute in the response MUST be provided by the
   * TSA in the certificates field from the SignedData structure in that
   * response. That field may also contain other certificates.
   *
   * If the certReq field is missing or if the certReq field is present
   * and set to false then the certificates field from the SignedData
   * structure MUST not be present in the response.
   */
  certReq?: boolean;
  /**
   * The extensions field is a generic way to add additional information
   * to the request in the future.
   */
  extensions?: Extension[];
}

export interface TimeStampReqJson {
  version: number;
  messageImprint: MessageImprintJson;
  reqPolicy?: string;
  nonce?: asn1js.IntegerJson;
  certReq?: boolean;
  extensions?: ExtensionJson[];
}

export type TimeStampReqParameters = PkiObjectParameters & Partial<ITimeStampReq>;

/**
 * Represents the TimeStampReq structure described in [RFC3161](https://www.ietf.org/rfc/rfc3161.txt)
 *
 * @example The following example demonstrates how to create Time-Stamp Request
 * ```js
 * const nonce = pkijs.getRandomValues(new Uint8Array(10)).buffer;
 *
 * const tspReq = new pkijs.TimeStampReq({
 *   version: 1,
 *   messageImprint: await pkijs.MessageImprint.create("SHA-256", message),
 *   reqPolicy: "1.2.3.4.5.6",
 *   certReq: true,
 *   nonce: new asn1js.Integer({ valueHex: nonce }),
 * });
 *
 * const tspReqRaw = tspReq.toSchema().toBER();
 */
export class TimeStampReq extends PkiObject implements ITimeStampReq {

  public static override CLASS_NAME = "TimeStampReq";

  public version!: number;
  public messageImprint!: MessageImprint;
  public reqPolicy?: string;
  public nonce?: asn1js.Integer;
  public certReq?: boolean;
  public extensions?: Extension[];

  /**
   * Initializes a new instance of the {@link TimeStampReq} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: TimeStampReqParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, TimeStampReq.defaultValues(VERSION));
    this.messageImprint = pvutils.getParametersValue(parameters, MESSAGE_IMPRINT, TimeStampReq.defaultValues(MESSAGE_IMPRINT));
    if (REQ_POLICY in parameters) {
      this.reqPolicy = pvutils.getParametersValue(parameters, REQ_POLICY, TimeStampReq.defaultValues(REQ_POLICY));
    }
    if (NONCE in parameters) {
      this.nonce = pvutils.getParametersValue(parameters, NONCE, TimeStampReq.defaultValues(NONCE));
    }
    if (CERT_REQ in parameters) {
      this.certReq = pvutils.getParametersValue(parameters, CERT_REQ, TimeStampReq.defaultValues(CERT_REQ));
    }
    if (EXTENSIONS in parameters) {
      this.extensions = pvutils.getParametersValue(parameters, EXTENSIONS, TimeStampReq.defaultValues(EXTENSIONS));
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
  public static override defaultValues(memberName: typeof MESSAGE_IMPRINT): MessageImprint;
  public static override defaultValues(memberName: typeof REQ_POLICY): string;
  public static override defaultValues(memberName: typeof NONCE): asn1js.Integer;
  public static override defaultValues(memberName: typeof CERT_REQ): boolean;
  public static override defaultValues(memberName: typeof EXTENSIONS): Extension[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case MESSAGE_IMPRINT:
        return new MessageImprint();
      case REQ_POLICY:
        return EMPTY_STRING;
      case NONCE:
        return new asn1js.Integer();
      case CERT_REQ:
        return false;
      case EXTENSIONS:
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
      case REQ_POLICY:
      case CERT_REQ:
        return (memberValue === TimeStampReq.defaultValues(memberName as typeof CERT_REQ));
      case MESSAGE_IMPRINT:
        return ((MessageImprint.compareWithDefault("hashAlgorithm", memberValue.hashAlgorithm)) &&
          (MessageImprint.compareWithDefault("hashedMessage", memberValue.hashedMessage)));
      case NONCE:
        return (memberValue.isEqual(TimeStampReq.defaultValues(memberName)));
      case EXTENSIONS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * TimeStampReq ::= SEQUENCE  {
   *    version               INTEGER  { v1(1) },
   *    messageImprint        MessageImprint,
   *    reqPolicy             TSAPolicyId              OPTIONAL,
   *    nonce                 INTEGER                  OPTIONAL,
   *    certReq               BOOLEAN                  DEFAULT FALSE,
   *    extensions            [0] IMPLICIT Extensions  OPTIONAL  }
   *
   * TSAPolicyId ::= OBJECT IDENTIFIER
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    messageImprint?: MessageImprintSchema;
    reqPolicy?: string;
    nonce?: string;
    certReq?: string;
    extensions?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || TIME_STAMP_REQ),
      value: [
        new asn1js.Integer({ name: (names.version || TIME_STAMP_REQ_VERSION) }),
        MessageImprint.schema(names.messageImprint || {
          names: {
            blockName: TIME_STAMP_REQ_MESSAGE_IMPRINT
          }
        }),
        new asn1js.ObjectIdentifier({
          name: (names.reqPolicy || TIME_STAMP_REQ_POLICY),
          optional: true
        }),
        new asn1js.Integer({
          name: (names.nonce || TIME_STAMP_REQ_NONCE),
          optional: true
        }),
        new asn1js.Boolean({
          name: (names.certReq || TIME_STAMP_REQ_CERT_REQ),
          optional: true
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Repeated({
            name: (names.extensions || TIME_STAMP_REQ_EXTENSIONS),
            value: Extension.schema()
          })]
        }) // IMPLICIT SEQUENCE value
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      TimeStampReq.schema()
    );

    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.version = asn1.result[TIME_STAMP_REQ_VERSION].valueBlock.valueDec;
    this.messageImprint = new MessageImprint({ schema: asn1.result[TIME_STAMP_REQ_MESSAGE_IMPRINT] });
    if (TIME_STAMP_REQ_POLICY in asn1.result)
      this.reqPolicy = asn1.result[TIME_STAMP_REQ_POLICY].valueBlock.toString();
    if (TIME_STAMP_REQ_NONCE in asn1.result)
      this.nonce = asn1.result[TIME_STAMP_REQ_NONCE];
    if (TIME_STAMP_REQ_CERT_REQ in asn1.result)
      this.certReq = asn1.result[TIME_STAMP_REQ_CERT_REQ].valueBlock.value;
    if (TIME_STAMP_REQ_EXTENSIONS in asn1.result)
      this.extensions = Array.from(asn1.result[TIME_STAMP_REQ_EXTENSIONS], element => new Extension({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(new asn1js.Integer({ value: this.version }));
    outputArray.push(this.messageImprint.toSchema());
    if (this.reqPolicy)
      outputArray.push(new asn1js.ObjectIdentifier({ value: this.reqPolicy }));
    if (this.nonce)
      outputArray.push(this.nonce);
    if ((CERT_REQ in this) && (TimeStampReq.compareWithDefault(CERT_REQ, this.certReq) === false))
      outputArray.push(new asn1js.Boolean({ value: this.certReq }));

    //#region Create array of extensions
    if (this.extensions) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: Array.from(this.extensions, o => o.toSchema())
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

  public toJSON(): TimeStampReqJson {
    const res: TimeStampReqJson = {
      version: this.version,
      messageImprint: this.messageImprint.toJSON()
    };

    if (this.reqPolicy !== undefined)
      res.reqPolicy = this.reqPolicy;

    if (this.nonce !== undefined)
      res.nonce = this.nonce.toJSON();

    if ((this.certReq !== undefined) && (TimeStampReq.compareWithDefault(CERT_REQ, this.certReq) === false))
      res.certReq = this.certReq;

    if (this.extensions) {
      res.extensions = Array.from(this.extensions, o => o.toJSON());
    }

    return res;
  }

}
