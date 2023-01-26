import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { Attribute, AttributeJson } from "./Attribute";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ATTRIBUTES = "attributes";
const CLEAR_PROPS = [
  ATTRIBUTES
];

export interface ISubjectDirectoryAttributes {
  attributes: Attribute[];
}

export interface SubjectDirectoryAttributesJson {
  attributes: AttributeJson[];
}

export type SubjectDirectoryAttributesParameters = PkiObjectParameters & Partial<ISubjectDirectoryAttributes>;

/**
 * Represents the SubjectDirectoryAttributes structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class SubjectDirectoryAttributes extends PkiObject implements ISubjectDirectoryAttributes {

  public static override CLASS_NAME = "SubjectDirectoryAttributes";

  public attributes!: Attribute[];

  /**
   * Initializes a new instance of the {@link SubjectDirectoryAttributes} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SubjectDirectoryAttributesParameters = {}) {
    super();

    this.attributes = pvutils.getParametersValue(parameters, ATTRIBUTES, SubjectDirectoryAttributes.defaultValues(ATTRIBUTES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ATTRIBUTES): Attribute[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ATTRIBUTES:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SubjectDirectoryAttributes ::= SEQUENCE SIZE (1..MAX) OF Attribute
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    attributes?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.attributes || EMPTY_STRING),
          value: Attribute.schema()
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
      SubjectDirectoryAttributes.schema({
        names: {
          attributes: ATTRIBUTES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.attributes = Array.from(asn1.result.attributes, element => new Attribute({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.attributes, o => o.toSchema())
    }));
  }

  public toJSON(): SubjectDirectoryAttributesJson {
    return {
      attributes: Array.from(this.attributes, o => o.toJSON())
    };
  }

}
