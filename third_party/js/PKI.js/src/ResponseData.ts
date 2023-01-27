import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import { RelativeDistinguishedNames, RelativeDistinguishedNamesSchema } from "./RelativeDistinguishedNames";
import { SingleResponse, SingleResponseJson, SingleResponseSchema } from "./SingleResponse";
import { Extension, ExtensionJson } from "./Extension";
import { Extensions, ExtensionsSchema } from "./Extensions";
import * as Schema from "./Schema";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_BUFFER } from "./constants";

const TBS = "tbs";
const VERSION = "version";
const RESPONDER_ID = "responderID";
const PRODUCED_AT = "producedAt";
const RESPONSES = "responses";
const RESPONSE_EXTENSIONS = "responseExtensions";
const RESPONSE_DATA = "ResponseData";
const RESPONSE_DATA_VERSION = `${RESPONSE_DATA}.${VERSION}`;
const RESPONSE_DATA_RESPONDER_ID = `${RESPONSE_DATA}.${RESPONDER_ID}`;
const RESPONSE_DATA_PRODUCED_AT = `${RESPONSE_DATA}.${PRODUCED_AT}`;
const RESPONSE_DATA_RESPONSES = `${RESPONSE_DATA}.${RESPONSES}`;
const RESPONSE_DATA_RESPONSE_EXTENSIONS = `${RESPONSE_DATA}.${RESPONSE_EXTENSIONS}`;
const CLEAR_PROPS = [
  RESPONSE_DATA,
  RESPONSE_DATA_VERSION,
  RESPONSE_DATA_RESPONDER_ID,
  RESPONSE_DATA_PRODUCED_AT,
  RESPONSE_DATA_RESPONSES,
  RESPONSE_DATA_RESPONSE_EXTENSIONS
];

export interface IResponseData {
  version?: number;
  tbs: ArrayBuffer;
  responderID: any;
  producedAt: Date;
  responses: SingleResponse[];
  responseExtensions?: Extension[];
}

export type ResponseDataParameters = PkiObjectParameters & Partial<IResponseData>;

export type ResponseDataSchema = Schema.SchemaParameters<{
  version?: string;
  responderID?: string;
  ResponseDataByName?: RelativeDistinguishedNamesSchema;
  ResponseDataByKey?: string;
  producedAt?: string;
  response?: SingleResponseSchema;
  extensions?: ExtensionsSchema;
}>;

export interface ResponseDataJson {
  version?: number;
  tbs: string;
  responderID: any;
  producedAt: Date;
  responses: SingleResponseJson[];
  responseExtensions?: ExtensionJson[];
}

/**
 * Represents an ResponseData described in [RFC6960](https://datatracker.ietf.org/doc/html/rfc6960)
 */
export class ResponseData extends PkiObject implements IResponseData {

  public static override CLASS_NAME = "ResponseData";

  public version?: number;
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
  public responderID: any;
  public producedAt!: Date;
  public responses!: SingleResponse[];
  public responseExtensions?: Extension[];

  /**
   * Initializes a new instance of the {@link ResponseData} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ResponseDataParameters = {}) {
    super();

    this.tbsView = new Uint8Array(pvutils.getParametersValue(parameters, TBS, ResponseData.defaultValues(TBS)));
    if (VERSION in parameters) {
      this.version = pvutils.getParametersValue(parameters, VERSION, ResponseData.defaultValues(VERSION));
    }
    this.responderID = pvutils.getParametersValue(parameters, RESPONDER_ID, ResponseData.defaultValues(RESPONDER_ID));
    this.producedAt = pvutils.getParametersValue(parameters, PRODUCED_AT, ResponseData.defaultValues(PRODUCED_AT));
    this.responses = pvutils.getParametersValue(parameters, RESPONSES, ResponseData.defaultValues(RESPONSES));
    if (RESPONSE_EXTENSIONS in parameters) {
      this.responseExtensions = pvutils.getParametersValue(parameters, RESPONSE_EXTENSIONS, ResponseData.defaultValues(RESPONSE_EXTENSIONS));
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
  public static override defaultValues(memberName: typeof TBS): ArrayBuffer;
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof RESPONDER_ID): any;
  public static override defaultValues(memberName: typeof PRODUCED_AT): Date;
  public static override defaultValues(memberName: typeof RESPONSES): SingleResponse[];
  public static override defaultValues(memberName: typeof RESPONSE_EXTENSIONS): Extension[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case TBS:
        return EMPTY_BUFFER;
      case RESPONDER_ID:
        return {};
      case PRODUCED_AT:
        return new Date(0, 0, 0);
      case RESPONSES:
      case RESPONSE_EXTENSIONS:
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
      // TODO version?
      case TBS:
        return (memberValue.byteLength === 0);
      case RESPONDER_ID:
        return (Object.keys(memberValue).length === 0);
      case PRODUCED_AT:
        return (memberValue === ResponseData.defaultValues(memberName));
      case RESPONSES:
      case RESPONSE_EXTENSIONS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * ResponseData ::= SEQUENCE {
   *    version              [0] EXPLICIT Version DEFAULT v1,
   *    responderID              ResponderID,
   *    producedAt               GeneralizedTime,
   *    responses                SEQUENCE OF SingleResponse,
   *    responseExtensions   [1] EXPLICIT Extensions OPTIONAL }
   *```
   */
  public static override schema(parameters: ResponseDataSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || RESPONSE_DATA),
      value: [
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Integer({ name: (names.version || RESPONSE_DATA_VERSION) })]
        }),
        new asn1js.Choice({
          value: [
            new asn1js.Constructed({
              name: (names.responderID || RESPONSE_DATA_RESPONDER_ID),
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 1 // [1]
              },
              value: [RelativeDistinguishedNames.schema(names.ResponseDataByName || {
                names: {
                  blockName: "ResponseData.byName"
                }
              })]
            }),
            new asn1js.Constructed({
              name: (names.responderID || RESPONSE_DATA_RESPONDER_ID),
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 2 // [2]
              },
              value: [new asn1js.OctetString({ name: (names.ResponseDataByKey || "ResponseData.byKey") })]
            })
          ]
        }),
        new asn1js.GeneralizedTime({ name: (names.producedAt || RESPONSE_DATA_PRODUCED_AT) }),
        new asn1js.Sequence({
          value: [
            new asn1js.Repeated({
              name: RESPONSE_DATA_RESPONSES,
              value: SingleResponse.schema(names.response || {})
            })
          ]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [Extensions.schema(names.extensions || {
            names: {
              blockName: RESPONSE_DATA_RESPONSE_EXTENSIONS
            }
          })]
        }) // EXPLICIT SEQUENCE value
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      ResponseData.schema()
    );

    AsnError.assertSchema(asn1, this.className);
    //#endregion

    //#region Get internal properties from parsed schema
    this.tbsView = (asn1.result.ResponseData as asn1js.Sequence).valueBeforeDecodeView;

    if (RESPONSE_DATA_VERSION in asn1.result)
      this.version = asn1.result[RESPONSE_DATA_VERSION].valueBlock.valueDec;

    if (asn1.result[RESPONSE_DATA_RESPONDER_ID].idBlock.tagNumber === 1)
      this.responderID = new RelativeDistinguishedNames({ schema: asn1.result[RESPONSE_DATA_RESPONDER_ID].valueBlock.value[0] });
    else
      this.responderID = asn1.result[RESPONSE_DATA_RESPONDER_ID].valueBlock.value[0]; // OCTET_STRING

    this.producedAt = asn1.result[RESPONSE_DATA_PRODUCED_AT].toDate();
    this.responses = Array.from(asn1.result[RESPONSE_DATA_RESPONSES], element => new SingleResponse({ schema: element }));

    if (RESPONSE_DATA_RESPONSE_EXTENSIONS in asn1.result)
      this.responseExtensions = Array.from(asn1.result[RESPONSE_DATA_RESPONSE_EXTENSIONS].valueBlock.value, element => new Extension({ schema: element }));
    //#endregion
  }

  public toSchema(encodeFlag = false): Schema.SchemaType {
    //#region Decode stored TBS value
    let tbsSchema;

    if (encodeFlag === false) {
      if (!this.tbsView.byteLength) {// No stored certificate TBS part
        return ResponseData.schema();
      }

      const asn1 = asn1js.fromBER(this.tbsView);
      AsnError.assert(asn1, "TBS Response Data");
      tbsSchema = asn1.result;
    }
    //#endregion
    //#region Create TBS schema via assembling from TBS parts
    else {
      const outputArray = [];

      if (VERSION in this) {
        outputArray.push(new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Integer({ value: this.version })]
        }));
      }

      if (this.responderID instanceof RelativeDistinguishedNames) {
        outputArray.push(new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [this.responderID.toSchema()]
        }));
      } else {
        outputArray.push(new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          },
          value: [this.responderID]
        }));
      }

      outputArray.push(new asn1js.GeneralizedTime({ valueDate: this.producedAt }));

      outputArray.push(new asn1js.Sequence({
        value: Array.from(this.responses, o => o.toSchema())
      }));

      if (this.responseExtensions) {
        outputArray.push(new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [new asn1js.Sequence({
            value: Array.from(this.responseExtensions, o => o.toSchema())
          })]
        }));
      }

      tbsSchema = new asn1js.Sequence({
        value: outputArray
      });
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return tbsSchema;
    //#endregion
  }

  public toJSON(): ResponseDataJson {
    const res = {} as ResponseDataJson;

    if (VERSION in this) {
      res.version = this.version;
    }

    if (this.responderID) {
      res.responderID = this.responderID;
    }

    if (this.producedAt) {
      res.producedAt = this.producedAt;
    }

    if (this.responses) {
      res.responses = Array.from(this.responses, o => o.toJSON());
    }

    if (this.responseExtensions) {
      res.responseExtensions = Array.from(this.responseExtensions, o => o.toJSON());
    }

    return res;
  }

}
