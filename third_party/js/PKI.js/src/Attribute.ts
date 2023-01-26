import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const TYPE = "type";
const VALUES = "values";
const CLEAR_PROPS = [
  TYPE,
  VALUES
];

export interface IAttribute {
  /**
   * Specifies type of attribute value
   */
  type: string;
  /**
   * List of attribute values
   */
  values: any[];
}

export type AttributeParameters = PkiObjectParameters & Partial<IAttribute>;

export type AttributeSchema = Schema.SchemaParameters<{
  setName?: string;
  type?: string;
  values?: string;
}>;

export interface AttributeJson {
  type: string;
  values: any[];
}

/**
 * Represents the Attribute structure described in [RFC2986](https://datatracker.ietf.org/doc/html/rfc2986)
 */
export class Attribute extends PkiObject implements IAttribute {

  public static override CLASS_NAME = "Attribute";

  public type!: string;
  public values!: any[];

  /**
   * Initializes a new instance of the {@link Attribute} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AttributeParameters = {}) {
    super();

    this.type = pvutils.getParametersValue(parameters, TYPE, Attribute.defaultValues(TYPE));
    this.values = pvutils.getParametersValue(parameters, VALUES, Attribute.defaultValues(VALUES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TYPE): string;
  public static override defaultValues(memberName: typeof VALUES): any[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TYPE:
        return EMPTY_STRING;
      case VALUES:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compares values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case TYPE:
        return (memberValue === EMPTY_STRING);
      case VALUES:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * Attribute { ATTRIBUTE:IOSet } ::= SEQUENCE {
   *    type   ATTRIBUTE.&id({IOSet}),
   *    values SET SIZE(1..MAX) OF ATTRIBUTE.&Type({IOSet}{@type})
   * }
   *```
   */
  public static override schema(parameters: AttributeSchema = {}) {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.type || EMPTY_STRING) }),
        new asn1js.Set({
          name: (names.setName || EMPTY_STRING),
          value: [
            new asn1js.Repeated({
              name: (names.values || EMPTY_STRING),
              value: new asn1js.Any()
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
      Attribute.schema({
        names: {
          type: TYPE,
          values: VALUES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.type = asn1.result.type.valueBlock.toString();
    this.values = asn1.result.values;
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.type }),
        new asn1js.Set({
          value: this.values
        })
      ]
    }));
    //#endregion
  }

  public toJSON(): AttributeJson {
    return {
      type: this.type,
      values: Array.from(this.values, o => o.toJSON())
    };
  }

}

