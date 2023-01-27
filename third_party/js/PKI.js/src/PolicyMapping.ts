import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ISSUER_DOMAIN_POLICY = "issuerDomainPolicy";
const SUBJECT_DOMAIN_POLICY = "subjectDomainPolicy";
const CLEAR_PROPS = [
  ISSUER_DOMAIN_POLICY,
  SUBJECT_DOMAIN_POLICY
];

export interface IPolicyMapping {
  issuerDomainPolicy: string;
  subjectDomainPolicy: string;
}

export interface PolicyMappingJson {
  issuerDomainPolicy: string;
  subjectDomainPolicy: string;
}

export type PolicyMappingParameters = PkiObjectParameters & Partial<IPolicyMapping>;

/**
 * Represents the PolicyMapping structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class PolicyMapping extends PkiObject implements IPolicyMapping {

  public static override CLASS_NAME = "PolicyMapping";

  public issuerDomainPolicy!: string;
  public subjectDomainPolicy!: string;

  /**
   * Initializes a new instance of the {@link PolicyMapping} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PolicyMappingParameters = {}) {
    super();

    this.issuerDomainPolicy = pvutils.getParametersValue(parameters, ISSUER_DOMAIN_POLICY, PolicyMapping.defaultValues(ISSUER_DOMAIN_POLICY));
    this.subjectDomainPolicy = pvutils.getParametersValue(parameters, SUBJECT_DOMAIN_POLICY, PolicyMapping.defaultValues(SUBJECT_DOMAIN_POLICY));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ISSUER_DOMAIN_POLICY): string;
  public static override defaultValues(memberName: typeof SUBJECT_DOMAIN_POLICY): string;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ISSUER_DOMAIN_POLICY:
        return EMPTY_STRING;
      case SUBJECT_DOMAIN_POLICY:
        return EMPTY_STRING;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PolicyMapping ::= SEQUENCE {
   *    issuerDomainPolicy      CertPolicyId,
   *    subjectDomainPolicy     CertPolicyId }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    issuerDomainPolicy?: string;
    subjectDomainPolicy?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.issuerDomainPolicy || EMPTY_STRING) }),
        new asn1js.ObjectIdentifier({ name: (names.subjectDomainPolicy || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      PolicyMapping.schema({
        names: {
          issuerDomainPolicy: ISSUER_DOMAIN_POLICY,
          subjectDomainPolicy: SUBJECT_DOMAIN_POLICY
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.issuerDomainPolicy = asn1.result.issuerDomainPolicy.valueBlock.toString();
    this.subjectDomainPolicy = asn1.result.subjectDomainPolicy.valueBlock.toString();
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.issuerDomainPolicy }),
        new asn1js.ObjectIdentifier({ value: this.subjectDomainPolicy })
      ]
    }));
  }

  public toJSON(): PolicyMappingJson {
    return {
      issuerDomainPolicy: this.issuerDomainPolicy,
      subjectDomainPolicy: this.subjectDomainPolicy
    };
  }

}
