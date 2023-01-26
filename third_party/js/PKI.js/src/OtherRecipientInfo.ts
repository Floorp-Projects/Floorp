import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ORI_TYPE = "oriType";
const ORI_VALUE = "oriValue";
const CLEAR_PROPS = [
  ORI_TYPE,
  ORI_VALUE
];

export interface IOtherRecipientInfo {
  oriType: string;
  oriValue: any;
}

export interface OtherRecipientInfoJson {
  oriType: string;
  oriValue?: any;
}

export type OtherRecipientInfoParameters = PkiObjectParameters & Partial<IOtherRecipientInfo>;

/**
 * Represents the OtherRecipientInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class OtherRecipientInfo extends PkiObject implements IOtherRecipientInfo {

  public static override CLASS_NAME = "OtherRecipientInfo";

  public oriType!: string;
  public oriValue: any;

  /**
   * Initializes a new instance of the {@link OtherRecipientInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OtherRecipientInfoParameters = {}) {
    super();

    this.oriType = pvutils.getParametersValue(parameters, ORI_TYPE, OtherRecipientInfo.defaultValues(ORI_TYPE));
    this.oriValue = pvutils.getParametersValue(parameters, ORI_VALUE, OtherRecipientInfo.defaultValues(ORI_VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  static override defaultValues(memberName: typeof ORI_TYPE): string;
  static override defaultValues(memberName: typeof ORI_VALUE): any;
  static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ORI_TYPE:
        return EMPTY_STRING;
      case ORI_VALUE:
        return {};
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
      case ORI_TYPE:
        return (memberValue === EMPTY_STRING);
      case ORI_VALUE:
        return (Object.keys(memberValue).length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * OtherRecipientInfo ::= SEQUENCE {
   *    oriType OBJECT IDENTIFIER,
   *    oriValue ANY DEFINED BY oriType }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    oriType?: string;
    oriValue?: string;
  }> = {}) {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.oriType || EMPTY_STRING) }),
        new asn1js.Any({ name: (names.oriValue || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      OtherRecipientInfo.schema({
        names: {
          oriType: ORI_TYPE,
          oriValue: ORI_VALUE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.oriType = asn1.result.oriType.valueBlock.toString();
    this.oriValue = asn1.result.oriValue;
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.oriType }),
        this.oriValue
      ]
    }));
    //#endregion
  }

  public toJSON(): OtherRecipientInfoJson {
    const res: OtherRecipientInfoJson = {
      oriType: this.oriType
    };

    if (!OtherRecipientInfo.compareWithDefault(ORI_VALUE, this.oriValue)) {
      res.oriValue = this.oriValue.toJSON();
    }

    return res;
  }

}
