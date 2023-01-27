import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const POLICY_QUALIFIER_ID = "policyQualifierId";
const QUALIFIER = "qualifier";
const CLEAR_PROPS = [
  POLICY_QUALIFIER_ID,
  QUALIFIER
];

export interface IPolicyQualifierInfo {
  policyQualifierId: string;
  qualifier: Schema.SchemaType;
}

export type PolicyQualifierInfoParameters = PkiObjectParameters & Partial<IPolicyQualifierInfo>;

export interface PolicyQualifierInfoJson {
  policyQualifierId: string;
  qualifier: any;
}

/**
 * Represents the PolicyQualifierInfo structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class PolicyQualifierInfo extends PkiObject implements IPolicyQualifierInfo {

  public static override CLASS_NAME = "PolicyQualifierInfo";

  public policyQualifierId!: string;
  public qualifier: Schema.SchemaType;

  /**
   * Initializes a new instance of the {@link PolicyQualifierInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PolicyQualifierInfoParameters = {}) {
    super();

    this.policyQualifierId = pvutils.getParametersValue(parameters, POLICY_QUALIFIER_ID, PolicyQualifierInfo.defaultValues(POLICY_QUALIFIER_ID));
    this.qualifier = pvutils.getParametersValue(parameters, QUALIFIER, PolicyQualifierInfo.defaultValues(QUALIFIER));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof POLICY_QUALIFIER_ID): string;
  public static override defaultValues(memberName: typeof QUALIFIER): asn1js.Any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case POLICY_QUALIFIER_ID:
        return EMPTY_STRING;
      case QUALIFIER:
        return new asn1js.Any();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PolicyQualifierInfo ::= SEQUENCE {
   *    policyQualifierId  PolicyQualifierId,
   *    qualifier          ANY DEFINED BY policyQualifierId }
   *
   * id-qt          OBJECT IDENTIFIER ::= { id-pkix 2 }
   * id-qt-cps      OBJECT IDENTIFIER ::= { id-qt 1 }
   * id-qt-unotice  OBJECT IDENTIFIER ::= { id-qt 2 }
   *
   * PolicyQualifierId ::= OBJECT IDENTIFIER ( id-qt-cps | id-qt-unotice )
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{ policyQualifierId?: string; qualifier?: string; }> = {}) {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.policyQualifierId || EMPTY_STRING) }),
        new asn1js.Any({ name: (names.qualifier || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      PolicyQualifierInfo.schema({
        names: {
          policyQualifierId: POLICY_QUALIFIER_ID,
          qualifier: QUALIFIER
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.policyQualifierId = asn1.result.policyQualifierId.valueBlock.toString();
    this.qualifier = asn1.result.qualifier;
  }

  public toSchema(): asn1js.Sequence {
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.policyQualifierId }),
        this.qualifier
      ]
    }));
  }

  public toJSON(): PolicyQualifierInfoJson {
    return {
      policyQualifierId: this.policyQualifierId,
      qualifier: this.qualifier.toJSON()
    };
  }

}
