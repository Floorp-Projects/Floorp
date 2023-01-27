import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { OtherKeyAttribute, OtherKeyAttributeJson, OtherKeyAttributeSchema } from "./OtherKeyAttribute";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const SUBJECT_KEY_IDENTIFIER = "subjectKeyIdentifier";
const DATE = "date";
const OTHER = "other";
const CLEAR_PROPS = [
  SUBJECT_KEY_IDENTIFIER,
  DATE,
  OTHER,
];

export interface IRecipientKeyIdentifier {
  subjectKeyIdentifier: asn1js.OctetString;
  date?: asn1js.GeneralizedTime;
  other?: OtherKeyAttribute;
}

export interface RecipientKeyIdentifierJson {
  subjectKeyIdentifier: asn1js.OctetStringJson;
  date?: asn1js.BaseBlockJson;
  other?: OtherKeyAttributeJson;
}

export type RecipientKeyIdentifierParameters = PkiObjectParameters & Partial<IRecipientKeyIdentifier>;

export type RecipientKeyIdentifierSchema = Schema.SchemaParameters<{
  subjectKeyIdentifier?: string;
  date?: string;
  other?: OtherKeyAttributeSchema;
}>;

/**
 * Represents the RecipientKeyIdentifier structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class RecipientKeyIdentifier extends PkiObject implements IRecipientKeyIdentifier {

  public static override CLASS_NAME = "RecipientKeyIdentifier";

  public subjectKeyIdentifier!: asn1js.OctetString;
  public date?: asn1js.GeneralizedTime;
  public other?: OtherKeyAttribute;

  /**
   * Initializes a new instance of the {@link RecipientKeyIdentifier} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RecipientKeyIdentifierParameters = {}) {
    super();

    this.subjectKeyIdentifier = pvutils.getParametersValue(parameters, SUBJECT_KEY_IDENTIFIER, RecipientKeyIdentifier.defaultValues(SUBJECT_KEY_IDENTIFIER));
    if (DATE in parameters) {
      this.date = pvutils.getParametersValue(parameters, DATE, RecipientKeyIdentifier.defaultValues(DATE));
    }
    if (OTHER in parameters) {
      this.other = pvutils.getParametersValue(parameters, OTHER, RecipientKeyIdentifier.defaultValues(OTHER));
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
  public static override defaultValues(memberName: typeof SUBJECT_KEY_IDENTIFIER): asn1js.OctetString;
  public static override defaultValues(memberName: typeof DATE): asn1js.GeneralizedTime;
  public static override defaultValues(memberName: typeof OTHER): OtherKeyAttribute;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case SUBJECT_KEY_IDENTIFIER:
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
      case SUBJECT_KEY_IDENTIFIER:
        return (memberValue.isEqual(RecipientKeyIdentifier.defaultValues(SUBJECT_KEY_IDENTIFIER)));
      case DATE:
        return ((memberValue.year === 0) &&
          (memberValue.month === 0) &&
          (memberValue.day === 0) &&
          (memberValue.hour === 0) &&
          (memberValue.minute === 0) &&
          (memberValue.second === 0) &&
          (memberValue.millisecond === 0));
      case OTHER:
        return ((memberValue.keyAttrId === EMPTY_STRING) && (("keyAttr" in memberValue) === false));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RecipientKeyIdentifier ::= SEQUENCE {
   *    subjectKeyIdentifier SubjectKeyIdentifier,
   *    date GeneralizedTime OPTIONAL,
   *    other OtherKeyAttribute OPTIONAL }
   *```
   */
  public static override schema(parameters: RecipientKeyIdentifierSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.OctetString({ name: (names.subjectKeyIdentifier || EMPTY_STRING) }),
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
      RecipientKeyIdentifier.schema({
        names: {
          subjectKeyIdentifier: SUBJECT_KEY_IDENTIFIER,
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
    this.subjectKeyIdentifier = asn1.result.subjectKeyIdentifier;

    if (DATE in asn1.result)
      this.date = asn1.result.date;

    if (OTHER in asn1.result)
      this.other = new OtherKeyAttribute({ schema: asn1.result.other });
  }

  public toSchema(): asn1js.Sequence {
    // Create array for output sequence
    const outputArray = [];

    outputArray.push(this.subjectKeyIdentifier);

    if (this.date) {
      outputArray.push(this.date);
    }

    if (this.other) {
      outputArray.push(this.other.toSchema());
    }

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toJSON(): RecipientKeyIdentifierJson {
    const res: any = {
      subjectKeyIdentifier: this.subjectKeyIdentifier.toJSON()
    };

    if (this.date) {
      res.date = this.date.toJSON();
    }

    if (this.other) {
      res.other = this.other.toJSON();
    }

    return res;
  }

}
