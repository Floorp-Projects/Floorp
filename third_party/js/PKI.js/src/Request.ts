import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { CertID, CertIDJson, CertIDSchema } from "./CertID";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { Extension, ExtensionJson, ExtensionSchema } from "./Extension";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const REQ_CERT = "reqCert";
const SINGLE_REQUEST_EXTENSIONS = "singleRequestExtensions";
const CLEAR_PROPS = [
  REQ_CERT,
  SINGLE_REQUEST_EXTENSIONS,
];

export interface IRequest {
  reqCert: CertID;
  singleRequestExtensions?: Extension[];
}

export interface RequestJson {
  reqCert: CertIDJson;
  singleRequestExtensions?: ExtensionJson[];
}

export type RequestParameters = PkiObjectParameters & Partial<IRequest>;

export type RequestSchema = Schema.SchemaParameters<{
  reqCert?: CertIDSchema;
  extensions?: ExtensionSchema;
  singleRequestExtensions?: string;
}>;

/**
 * Represents an Request described in [RFC6960](https://datatracker.ietf.org/doc/html/rfc6960)
 */
export class Request extends PkiObject implements IRequest {

  public static override CLASS_NAME = "Request";

  public reqCert!: CertID;
  public singleRequestExtensions?: Extension[];

  /**
   * Initializes a new instance of the {@link Request} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RequestParameters = {}) {
    super();

    this.reqCert = pvutils.getParametersValue(parameters, REQ_CERT, Request.defaultValues(REQ_CERT));
    if (SINGLE_REQUEST_EXTENSIONS in parameters) {
      this.singleRequestExtensions = pvutils.getParametersValue(parameters, SINGLE_REQUEST_EXTENSIONS, Request.defaultValues(SINGLE_REQUEST_EXTENSIONS));
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
  public static override defaultValues(memberName: typeof REQ_CERT): CertID;
  public static override defaultValues(memberName: typeof SINGLE_REQUEST_EXTENSIONS): Extension[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case REQ_CERT:
        return new CertID();
      case SINGLE_REQUEST_EXTENSIONS:
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
      case REQ_CERT:
        return (memberValue.isEqual(Request.defaultValues(memberName)));
      case SINGLE_REQUEST_EXTENSIONS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * Request ::= SEQUENCE {
   *    reqCert                     CertID,
   *    singleRequestExtensions     [0] EXPLICIT Extensions OPTIONAL }
   *```
   */
  public static override schema(parameters: RequestSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        CertID.schema(names.reqCert || {}),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [Extension.schema(names.extensions || {
            names: {
              blockName: (names.singleRequestExtensions || EMPTY_STRING)
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
      Request.schema({
        names: {
          reqCert: {
            names: {
              blockName: REQ_CERT
            }
          },
          extensions: {
            names: {
              blockName: SINGLE_REQUEST_EXTENSIONS
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.reqCert = new CertID({ schema: asn1.result.reqCert });
    if (SINGLE_REQUEST_EXTENSIONS in asn1.result) {
      this.singleRequestExtensions = Array.from(asn1.result.singleRequestExtensions.valueBlock.value, element => new Extension({ schema: element }));
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.reqCert.toSchema());

    if (this.singleRequestExtensions) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          new asn1js.Sequence({
            value: Array.from(this.singleRequestExtensions, o => o.toSchema())
          })
        ]
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): RequestJson {
    const res: RequestJson = {
      reqCert: this.reqCert.toJSON()
    };

    if (this.singleRequestExtensions) {
      res.singleRequestExtensions = Array.from(this.singleRequestExtensions, o => o.toJSON());
    }

    return res;
  }

}

