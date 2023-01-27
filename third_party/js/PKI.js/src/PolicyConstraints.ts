import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const REQUIRE_EXPLICIT_POLICY = "requireExplicitPolicy";
const INHIBIT_POLICY_MAPPING = "inhibitPolicyMapping";
const CLEAR_PROPS = [
  REQUIRE_EXPLICIT_POLICY,
  INHIBIT_POLICY_MAPPING,
];

export interface IPolicyConstraints {
  requireExplicitPolicy?: number;
  inhibitPolicyMapping?: number;
}

export interface PolicyConstraintsJson {
  requireExplicitPolicy?: number;
  inhibitPolicyMapping?: number;
}

export type PolicyConstraintsParameters = PkiObjectParameters & Partial<IPolicyConstraints>;

/**
 * Represents the PolicyConstraints structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class PolicyConstraints extends PkiObject implements IPolicyConstraints {

  public static override CLASS_NAME = "PolicyConstraints";

  public requireExplicitPolicy?: number;
  public inhibitPolicyMapping?: number;

  /**
   * Initializes a new instance of the {@link PolicyConstraints} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PolicyConstraintsParameters = {}) {
    super();

    if (REQUIRE_EXPLICIT_POLICY in parameters) {
      this.requireExplicitPolicy = pvutils.getParametersValue(parameters, REQUIRE_EXPLICIT_POLICY, PolicyConstraints.defaultValues(REQUIRE_EXPLICIT_POLICY));
    }
    if (INHIBIT_POLICY_MAPPING in parameters) {
      this.inhibitPolicyMapping = pvutils.getParametersValue(parameters, INHIBIT_POLICY_MAPPING, PolicyConstraints.defaultValues(INHIBIT_POLICY_MAPPING));
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
  public static override defaultValues(memberName: typeof REQUIRE_EXPLICIT_POLICY): number;
  public static override defaultValues(memberName: typeof INHIBIT_POLICY_MAPPING): number;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case REQUIRE_EXPLICIT_POLICY:
        return 0;
      case INHIBIT_POLICY_MAPPING:
        return 0;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PolicyConstraints ::= SEQUENCE {
   *    requireExplicitPolicy           [0] SkipCerts OPTIONAL,
   *    inhibitPolicyMapping            [1] SkipCerts OPTIONAL }
   *
   * SkipCerts ::= INTEGER (0..MAX)
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    requireExplicitPolicy?: string;
    inhibitPolicyMapping?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Primitive({
          name: (names.requireExplicitPolicy || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          }
        }), // IMPLICIT integer value
        new asn1js.Primitive({
          name: (names.inhibitPolicyMapping || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          }
        }) // IMPLICIT integer value
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      PolicyConstraints.schema({
        names: {
          requireExplicitPolicy: REQUIRE_EXPLICIT_POLICY,
          inhibitPolicyMapping: INHIBIT_POLICY_MAPPING
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    if (REQUIRE_EXPLICIT_POLICY in asn1.result) {
      const field1 = asn1.result.requireExplicitPolicy;

      field1.idBlock.tagClass = 1; // UNIVERSAL
      field1.idBlock.tagNumber = 2; // INTEGER

      const ber1 = field1.toBER(false);
      const int1 = asn1js.fromBER(ber1);
      AsnError.assert(int1, "Integer");

      this.requireExplicitPolicy = (int1.result as asn1js.Integer).valueBlock.valueDec;
    }

    if (INHIBIT_POLICY_MAPPING in asn1.result) {
      const field2 = asn1.result.inhibitPolicyMapping;

      field2.idBlock.tagClass = 1; // UNIVERSAL
      field2.idBlock.tagNumber = 2; // INTEGER

      const ber2 = field2.toBER(false);
      const int2 = asn1js.fromBER(ber2);
      AsnError.assert(int2, "Integer");

      this.inhibitPolicyMapping = (int2.result as asn1js.Integer).valueBlock.valueDec;
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create correct values for output sequence
    const outputArray = [];

    if (REQUIRE_EXPLICIT_POLICY in this) {
      const int1 = new asn1js.Integer({ value: this.requireExplicitPolicy });

      int1.idBlock.tagClass = 3; // CONTEXT-SPECIFIC
      int1.idBlock.tagNumber = 0; // [0]

      outputArray.push(int1);
    }

    if (INHIBIT_POLICY_MAPPING in this) {
      const int2 = new asn1js.Integer({ value: this.inhibitPolicyMapping });

      int2.idBlock.tagClass = 3; // CONTEXT-SPECIFIC
      int2.idBlock.tagNumber = 1; // [1]

      outputArray.push(int2);
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): PolicyConstraintsJson {
    const res: PolicyConstraintsJson = {};

    if (REQUIRE_EXPLICIT_POLICY in this) {
      res.requireExplicitPolicy = this.requireExplicitPolicy;
    }

    if (INHIBIT_POLICY_MAPPING in this) {
      res.inhibitPolicyMapping = this.inhibitPolicyMapping;
    }

    return res;
  }

}
