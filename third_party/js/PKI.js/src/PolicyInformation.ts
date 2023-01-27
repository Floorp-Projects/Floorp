import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { PolicyQualifierInfo, PolicyQualifierInfoJson } from "./PolicyQualifierInfo";
import * as Schema from "./Schema";

export const POLICY_IDENTIFIER = "policyIdentifier";
export const POLICY_QUALIFIERS = "policyQualifiers";
const CLEAR_PROPS = [
  POLICY_IDENTIFIER,
  POLICY_QUALIFIERS
];

export interface IPolicyInformation {
  policyIdentifier: string;
  policyQualifiers?: PolicyQualifierInfo[];
}

export type PolicyInformationParameters = PkiObjectParameters & Partial<IPolicyInformation>;

export interface PolicyInformationJson {
  policyIdentifier: string;
  policyQualifiers?: PolicyQualifierInfoJson[];
}

/**
 * Represents the PolicyInformation structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class PolicyInformation extends PkiObject implements IPolicyInformation {

  public static override CLASS_NAME = "PolicyInformation";

  public policyIdentifier!: string;
  public policyQualifiers?: PolicyQualifierInfo[];

  /**
   * Initializes a new instance of the {@link PolicyInformation} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PolicyInformationParameters = {}) {
    super();

    this.policyIdentifier = pvutils.getParametersValue(parameters, POLICY_IDENTIFIER, PolicyInformation.defaultValues(POLICY_IDENTIFIER));
    if (POLICY_QUALIFIERS in parameters) {
      this.policyQualifiers = pvutils.getParametersValue(parameters, POLICY_QUALIFIERS, PolicyInformation.defaultValues(POLICY_QUALIFIERS));
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
  public static override defaultValues(memberName: typeof POLICY_IDENTIFIER): string;
  public static override defaultValues(memberName: typeof POLICY_QUALIFIERS): PolicyQualifierInfo[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case POLICY_IDENTIFIER:
        return EMPTY_STRING;
      case POLICY_QUALIFIERS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PolicyInformation ::= SEQUENCE {
   *    policyIdentifier   CertPolicyId,
   *    policyQualifiers   SEQUENCE SIZE (1..MAX) OF
   *    PolicyQualifierInfo OPTIONAL }
   *
   * CertPolicyId ::= OBJECT IDENTIFIER
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{ policyIdentifier?: string; policyQualifiers?: string; }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.policyIdentifier || EMPTY_STRING) }),
        new asn1js.Sequence({
          optional: true,
          value: [
            new asn1js.Repeated({
              name: (names.policyQualifiers || EMPTY_STRING),
              value: PolicyQualifierInfo.schema()
            })
          ]
        })
      ]
    }));
  }

  /**
   * Converts parsed ASN.1 object into current class
   * @param schema
   */
  fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      PolicyInformation.schema({
        names: {
          policyIdentifier: POLICY_IDENTIFIER,
          policyQualifiers: POLICY_QUALIFIERS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.policyIdentifier = asn1.result.policyIdentifier.valueBlock.toString();
    if (POLICY_QUALIFIERS in asn1.result) {
      this.policyQualifiers = Array.from(asn1.result.policyQualifiers, element => new PolicyQualifierInfo({ schema: element }));
    }
  }

  public toSchema(): asn1js.Sequence {
    //Create array for output sequence
    const outputArray = [];

    outputArray.push(new asn1js.ObjectIdentifier({ value: this.policyIdentifier }));

    if (this.policyQualifiers) {
      outputArray.push(new asn1js.Sequence({
        value: Array.from(this.policyQualifiers, o => o.toSchema())
      }));
    }

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toJSON(): PolicyInformationJson {
    const res: PolicyInformationJson = {
      policyIdentifier: this.policyIdentifier
    };

    if (this.policyQualifiers)
      res.policyQualifiers = Array.from(this.policyQualifiers, o => o.toJSON());

    return res;
  }

}
