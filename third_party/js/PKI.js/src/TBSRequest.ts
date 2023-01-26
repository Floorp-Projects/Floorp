import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import { GeneralName, GeneralNameJson, GeneralNameSchema } from "./GeneralName";
import { Request, RequestJson, RequestSchema } from "./Request";
import { Extension, ExtensionJson } from "./Extension";
import { Extensions, ExtensionsSchema } from "./Extensions";
import * as Schema from "./Schema";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_BUFFER } from "./constants";

const TBS = "tbs";
const VERSION = "version";
const REQUESTOR_NAME = "requestorName";
const REQUEST_LIST = "requestList";
const REQUEST_EXTENSIONS = "requestExtensions";
const TBS_REQUEST = "TBSRequest";
const TBS_REQUEST_VERSION = `${TBS_REQUEST}.${VERSION}`;
const TBS_REQUEST_REQUESTOR_NAME = `${TBS_REQUEST}.${REQUESTOR_NAME}`;
const TBS_REQUEST_REQUESTS = `${TBS_REQUEST}.requests`;
const TBS_REQUEST_REQUEST_EXTENSIONS = `${TBS_REQUEST}.${REQUEST_EXTENSIONS}`;
const CLEAR_PROPS = [
  TBS_REQUEST,
  TBS_REQUEST_VERSION,
  TBS_REQUEST_REQUESTOR_NAME,
  TBS_REQUEST_REQUESTS,
  TBS_REQUEST_REQUEST_EXTENSIONS
];

export interface ITBSRequest {
  tbs: ArrayBuffer;
  version?: number;
  requestorName?: GeneralName;
  requestList: Request[];
  requestExtensions?: Extension[];
}

export interface TBSRequestJson {
  tbs: string;
  version?: number;
  requestorName?: GeneralNameJson;
  requestList: RequestJson[];
  requestExtensions?: ExtensionJson[];
}

export type TBSRequestParameters = PkiObjectParameters & Partial<ITBSRequest>;

export type TBSRequestSchema = Schema.SchemaParameters<{
  TBSRequestVersion?: string;
  requestorName?: GeneralNameSchema;
  requestList?: string;
  requests?: string;
  requestNames?: RequestSchema;
  extensions?: ExtensionsSchema;
  requestExtensions?: string;
}>;

/**
 * Represents the TBSRequest structure described in [RFC6960](https://datatracker.ietf.org/doc/html/rfc6960)
 */
export class TBSRequest extends PkiObject implements ITBSRequest {

  public static override CLASS_NAME = "TBSRequest";

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
  public version?: number;
  public requestorName?: GeneralName;
  public requestList!: Request[];
  public requestExtensions?: Extension[];

  /**
   * Initializes a new instance of the {@link TBSRequest} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: TBSRequestParameters = {}) {
    super();

    this.tbsView = new Uint8Array(pvutils.getParametersValue(parameters, TBS, TBSRequest.defaultValues(TBS)));
    if (VERSION in parameters) {
      this.version = pvutils.getParametersValue(parameters, VERSION, TBSRequest.defaultValues(VERSION));
    }
    if (REQUESTOR_NAME in parameters) {
      this.requestorName = pvutils.getParametersValue(parameters, REQUESTOR_NAME, TBSRequest.defaultValues(REQUESTOR_NAME));
    }
    this.requestList = pvutils.getParametersValue(parameters, REQUEST_LIST, TBSRequest.defaultValues(REQUEST_LIST));
    if (REQUEST_EXTENSIONS in parameters) {
      this.requestExtensions = pvutils.getParametersValue(parameters, REQUEST_EXTENSIONS, TBSRequest.defaultValues(REQUEST_EXTENSIONS));
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
  public static override defaultValues(memberName: typeof REQUESTOR_NAME): GeneralName;
  public static override defaultValues(memberName: typeof REQUEST_LIST): Request[];
  public static override defaultValues(memberName: typeof REQUEST_EXTENSIONS): Extension[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TBS:
        return EMPTY_BUFFER;
      case VERSION:
        return 0;
      case REQUESTOR_NAME:
        return new GeneralName();
      case REQUEST_LIST:
      case REQUEST_EXTENSIONS:
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
      case TBS:
        return (memberValue.byteLength === 0);
      case VERSION:
        return (memberValue === TBSRequest.defaultValues(memberName));
      case REQUESTOR_NAME:
        return ((memberValue.type === GeneralName.defaultValues("type")) && (Object.keys(memberValue.value).length === 0));
      case REQUEST_LIST:
      case REQUEST_EXTENSIONS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * TBSRequest ::= SEQUENCE {
   *    version             [0]     EXPLICIT Version DEFAULT v1,
   *    requestorName       [1]     EXPLICIT GeneralName OPTIONAL,
   *    requestList                 SEQUENCE OF Request,
   *    requestExtensions   [2]     EXPLICIT Extensions OPTIONAL }
   *```
   */
  public static override schema(parameters: TBSRequestSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || TBS_REQUEST),
      value: [
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Integer({ name: (names.TBSRequestVersion || TBS_REQUEST_VERSION) })]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [GeneralName.schema(names.requestorName || {
            names: {
              blockName: TBS_REQUEST_REQUESTOR_NAME
            }
          })]
        }),
        new asn1js.Sequence({
          name: (names.requestList || "TBSRequest.requestList"),
          value: [
            new asn1js.Repeated({
              name: (names.requests || TBS_REQUEST_REQUESTS),
              value: Request.schema(names.requestNames || {})
            })
          ]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          },
          value: [Extensions.schema(names.extensions || {
            names: {
              blockName: (names.requestExtensions || TBS_REQUEST_REQUEST_EXTENSIONS)
            }
          })]
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
      TBSRequest.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.tbsView = asn1.result.TBSRequest.valueBeforeDecodeView;

    if (TBS_REQUEST_VERSION in asn1.result)
      this.version = asn1.result[TBS_REQUEST_VERSION].valueBlock.valueDec;
    if (TBS_REQUEST_REQUESTOR_NAME in asn1.result)
      this.requestorName = new GeneralName({ schema: asn1.result[TBS_REQUEST_REQUESTOR_NAME] });

    this.requestList = Array.from(asn1.result[TBS_REQUEST_REQUESTS], element => new Request({ schema: element }));

    if (TBS_REQUEST_REQUEST_EXTENSIONS in asn1.result)
      this.requestExtensions = Array.from(asn1.result[TBS_REQUEST_REQUEST_EXTENSIONS].valueBlock.value, element => new Extension({ schema: element }));
  }

  /**
   * Convert current object to asn1js object and set correct values
   * @param encodeFlag If param equal to false then create TBS schema via decoding stored value. In othe case create TBS schema via assembling from TBS parts.
   * @returns asn1js object
   */
  public toSchema(encodeFlag = false): asn1js.Sequence {
    //#region Decode stored TBS value
    let tbsSchema;

    if (encodeFlag === false) {
      if (this.tbsView.byteLength === 0) // No stored TBS part
        return TBSRequest.schema();

      const asn1 = asn1js.fromBER(this.tbsView);
      AsnError.assert(asn1, "TBS Request");
      if (!(asn1.result instanceof asn1js.Sequence)) {
        throw new Error("ASN.1 result should be SEQUENCE");
      }

      tbsSchema = asn1.result;
    }
    //#endregion
    //#region Create TBS schema via assembling from TBS parts
    else {
      const outputArray = [];

      if (this.version !== undefined) {
        outputArray.push(new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Integer({ value: this.version })]
        }));
      }

      if (this.requestorName) {
        outputArray.push(new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [this.requestorName.toSchema()]
        }));
      }

      outputArray.push(new asn1js.Sequence({
        value: Array.from(this.requestList, o => o.toSchema())
      }));

      if (this.requestExtensions) {
        outputArray.push(new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          },
          value: [
            new asn1js.Sequence({
              value: Array.from(this.requestExtensions, o => o.toSchema())
            })
          ]
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

  public toJSON(): TBSRequestJson {
    const res: any = {};

    if (this.version != undefined)
      res.version = this.version;

    if (this.requestorName) {
      res.requestorName = this.requestorName.toJSON();
    }

    res.requestList = Array.from(this.requestList, o => o.toJSON());

    if (this.requestExtensions) {
      res.requestExtensions = Array.from(this.requestExtensions, o => o.toJSON());
    }

    return res;
  }

}
