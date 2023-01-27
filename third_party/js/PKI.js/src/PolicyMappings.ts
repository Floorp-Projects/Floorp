import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { PolicyMapping, PolicyMappingJson } from "./PolicyMapping";
import * as Schema from "./Schema";

const MAPPINGS = "mappings";
const CLEAR_PROPS = [
  MAPPINGS,
];

export interface IPolicyMappings {
  mappings: PolicyMapping[];
}

export interface PolicyMappingsJson {
  mappings: PolicyMappingJson[];
}

export type PolicyMappingsParameters = PkiObjectParameters & Partial<IPolicyMappings>;

/**
 * Represents the PolicyMappings structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class PolicyMappings extends PkiObject implements IPolicyMappings {

  public static override CLASS_NAME = "PolicyMappings";

  public mappings!: PolicyMapping[];

  /**
   * Initializes a new instance of the {@link PolicyMappings} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PolicyMappingsParameters = {}) {
    super();

    this.mappings = pvutils.getParametersValue(parameters, MAPPINGS, PolicyMappings.defaultValues(MAPPINGS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: string): PolicyMapping[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case MAPPINGS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF PolicyMapping
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    mappings?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.mappings || EMPTY_STRING),
          value: PolicyMapping.schema()
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
      PolicyMappings.schema({
        names: {
          mappings: MAPPINGS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.mappings = Array.from(asn1.result.mappings, element => new PolicyMapping({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.mappings, o => o.toSchema())
    }));
  }

  public toJSON(): PolicyMappingsJson {
    return {
      mappings: Array.from(this.mappings, o => o.toJSON())
    };
  }

}
