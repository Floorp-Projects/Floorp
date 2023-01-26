import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { OtherKeyAttribute, OtherKeyAttributeJson, OtherKeyAttributeSchema } from "./OtherKeyAttribute";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const KEY_IDENTIFIER = "keyIdentifier";
const DATE = "date";
const OTHER = "other";
const CLEAR_PROPS = [
  KEY_IDENTIFIER,
  DATE,
  OTHER,
];

export interface IKEKIdentifier {
  keyIdentifier: asn1js.OctetString;
  date?: asn1js.GeneralizedTime;
  other?: OtherKeyAttribute;
}

export interface KEKIdentifierJson {
  keyIdentifier: asn1js.OctetStringJson;
  date?: asn1js.GeneralizedTime;
  other?: OtherKeyAttributeJson;
}

export type KEKIdentifierParameters = PkiObjectParameters & Partial<IKEKIdentifier>;

export type KEKIdentifierSchema = Schema.SchemaParameters<{
  keyIdentifier?: string;
  date?: string;
  other?: OtherKeyAttributeSchema;
}>;

/**
 * Represents the KEKIdentifier structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class KEKIdentifier extends PkiObject implements IKEKIdentifier {

  public static override CLASS_NAME = "KEKIdentifier";

  public keyIdentifier!: asn1js.OctetString;
  public date?: asn1js.GeneralizedTime;
  public other?: OtherKeyAttribute;

  /**
   * Initializes a new instance of the {@link KEKIdentifier} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: KEKIdentifierParameters = {}) {
    super();

    this.keyIdentifier = pvutils.getParametersValue(parameters, KEY_IDENTIFIER, KEKIdentifier.defaultValues(KEY_IDENTIFIER));
    if (DATE in parameters) {
      this.date = pvutils.getParametersValue(parameters, DATE, KEKIdentifier.defaultValues(DATE));
    }
    if (OTHER in parameters) {
      this.other = pvutils.getParametersValue(parameters, OTHER, KEKIdentifier.defaultValues(OTHER));
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
  public static override defaultValues(memberName: typeof KEY_IDENTIFIER): asn1js.OctetString;
  public static override defaultValues(memberName: typeof DATE): asn1js.GeneralizedTime;
  public static override defaultValues(memberName: typeof OTHER): OtherKeyAttribute;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case KEY_IDENTIFIER:
        return new asn1js.OctetString();
      case DATE:
        return new asn1js.GeneralizedTime();
      case OTHER:
        return new OtherKeyAttribute();
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
      case KEY_IDENTIFIER:
        return (memberValue.isEqual(KEKIdentifier.defaultValues(KEY_IDENTIFIER)));
      case DATE:
        return ((memberValue.year === 0) &&
          (memberValue.month === 0) &&
          (memberValue.day === 0) &&
          (memberValue.hour === 0) &&
          (memberValue.minute === 0) &&
          (memberValue.second === 0) &&
          (memberValue.millisecond === 0));
      case OTHER:
        return ((memberValue.compareWithDefault("keyAttrId", memberValue.keyAttrId)) &&
          (("keyAttr" in memberValue) === false));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * KEKIdentifier ::= SEQUENCE {
   *    keyIdentifier OCTET STRING,
   *    date GeneralizedTime OPTIONAL,
   *    other OtherKeyAttribute OPTIONAL }
   *```
   */
  public static override schema(parameters: KEKIdentifierSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.OctetString({ name: (names.keyIdentifier || EMPTY_STRING) }),
        new asn1js.GeneralizedTime({
          optional: true,
          name: (names.date || EMPTY_STRING)
        }),
        OtherKeyAttribute.schema(names.other || {})
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      KEKIdentifier.schema({
        names: {
          keyIdentifier: KEY_IDENTIFIER,
          date: DATE,
          other: {
            names: {
              blockName: OTHER
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.keyIdentifier = asn1.result.keyIdentifier;
    if (DATE in asn1.result)
      this.date = asn1.result.date;
    if (OTHER in asn1.result)
      this.other = new OtherKeyAttribute({ schema: asn1.result.other });
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.keyIdentifier);

    if (this.date) {
      outputArray.push(this.date);
    }
    if (this.other) {
      outputArray.push(this.other.toSchema());
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): KEKIdentifierJson {
    const res: KEKIdentifierJson = {
      keyIdentifier: this.keyIdentifier.toJSON()
    };

    if (this.date) {
      res.date = this.date;
    }

    if (this.other) {
      res.other = this.other.toJSON();
    }

    return res;
  }

}
