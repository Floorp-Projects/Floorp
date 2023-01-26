import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const RESPONSE_TYPE = "responseType";
const RESPONSE = "response";
const CLEAR_PROPS = [
  RESPONSE_TYPE,
  RESPONSE
];

export interface IResponseBytes {
  responseType: string;
  response: asn1js.OctetString;
}

export interface ResponseBytesJson {
  responseType: string;
  response: asn1js.OctetStringJson;
}

export type ResponseBytesParameters = PkiObjectParameters & Partial<IResponseBytes>;

export type ResponseBytesSchema = Schema.SchemaParameters<{
  responseType?: string;
  response?: string;
}>;

/**
 * Class from RFC6960
 */
export class ResponseBytes extends PkiObject implements IResponseBytes {

  public static override CLASS_NAME = "ResponseBytes";

  public responseType!: string;
  public response!: asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link Request} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ResponseBytesParameters = {}) {
    super();

    this.responseType = pvutils.getParametersValue(parameters, RESPONSE_TYPE, ResponseBytes.defaultValues(RESPONSE_TYPE));
    this.response = pvutils.getParametersValue(parameters, RESPONSE, ResponseBytes.defaultValues(RESPONSE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof RESPONSE_TYPE): string;
  public static override defaultValues(memberName: typeof RESPONSE): asn1js.OctetString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case RESPONSE_TYPE:
        return EMPTY_STRING;
      case RESPONSE:
        return new asn1js.OctetString();
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
      case RESPONSE_TYPE:
        return (memberValue === EMPTY_STRING);
      case RESPONSE:
        return (memberValue.isEqual(ResponseBytes.defaultValues(memberName)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * ResponseBytes ::= SEQUENCE {
   *    responseType   OBJECT IDENTIFIER,
   *    response       OCTET STRING }
   *```
   */
  public static override schema(parameters: ResponseBytesSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.responseType || EMPTY_STRING) }),
        new asn1js.OctetString({ name: (names.response || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      ResponseBytes.schema({
        names: {
          responseType: RESPONSE_TYPE,
          response: RESPONSE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.responseType = asn1.result.responseType.valueBlock.toString();
    this.response = asn1.result.response;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.responseType }),
        this.response
      ]
    }));
  }

  public toJSON(): ResponseBytesJson {
    return {
      responseType: this.responseType,
      response: this.response.toJSON(),
    };
  }

}
