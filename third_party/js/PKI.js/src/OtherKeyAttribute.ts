import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const KEY_ATTR_ID = "keyAttrId";
const KEY_ATTR = "keyAttr";
const CLEAR_PROPS = [
  KEY_ATTR_ID,
  KEY_ATTR,
];

export interface IOtherKeyAttribute {
  keyAttrId: string;
  keyAttr?: any;
}

export interface OtherKeyAttributeJson {
  keyAttrId: string;
  keyAttr?: any;
}

export type OtherKeyAttributeParameters = PkiObjectParameters & Partial<IOtherKeyAttribute>;

export type OtherKeyAttributeSchema = Schema.SchemaType;

/**
 * Represents the OtherKeyAttribute structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class OtherKeyAttribute extends PkiObject implements IOtherKeyAttribute {

  public static override CLASS_NAME = "OtherKeyAttribute";

  public keyAttrId!: string;
  public keyAttr?: any;

  /**
   * Initializes a new instance of the {@link OtherKeyAttribute} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OtherKeyAttributeParameters = {}) {
    super();

    this.keyAttrId = pvutils.getParametersValue(parameters, KEY_ATTR_ID, OtherKeyAttribute.defaultValues(KEY_ATTR_ID));
    if (KEY_ATTR in parameters) {
      this.keyAttr = pvutils.getParametersValue(parameters, KEY_ATTR, OtherKeyAttribute.defaultValues(KEY_ATTR));
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
  public static override defaultValues(memberName: typeof KEY_ATTR_ID): string;
  public static override defaultValues(memberName: typeof KEY_ATTR): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case KEY_ATTR_ID:
        return EMPTY_STRING;
      case KEY_ATTR:
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
  public static compareWithDefault<T>(memberName: string, memberValue: T): memberValue is T {
    switch (memberName) {
      case KEY_ATTR_ID:
        return (typeof memberValue === "string" && memberValue === EMPTY_STRING);
      case KEY_ATTR:
        return (Object.keys(memberValue).length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * OtherKeyAttribute ::= SEQUENCE {
   *    keyAttrId OBJECT IDENTIFIER,
   *    keyAttr ANY DEFINED BY keyAttrId OPTIONAL }
   *```
   */
  public static override schema(parameters: OtherKeyAttributeSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      optional: (names.optional || true),
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.keyAttrId || EMPTY_STRING) }),
        new asn1js.Any({
          optional: true,
          name: (names.keyAttr || EMPTY_STRING)
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
      OtherKeyAttribute.schema({
        names: {
          keyAttrId: KEY_ATTR_ID,
          keyAttr: KEY_ATTR
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.keyAttrId = asn1.result.keyAttrId.valueBlock.toString();
    if (KEY_ATTR in asn1.result) {
      this.keyAttr = asn1.result.keyAttr;
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(new asn1js.ObjectIdentifier({ value: this.keyAttrId }));

    if (KEY_ATTR in this) {
      outputArray.push(this.keyAttr);
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray,
    }));
    //#endregion
  }

  public toJSON(): OtherKeyAttributeJson {
    const res: OtherKeyAttributeJson = {
      keyAttrId: this.keyAttrId
    };

    if (KEY_ATTR in this) {
      res.keyAttr = this.keyAttr.toJSON();
    }

    return res;
  }

}
