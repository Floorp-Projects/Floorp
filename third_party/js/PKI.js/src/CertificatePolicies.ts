import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { PolicyInformation, PolicyInformationJson } from "./PolicyInformation";
import * as Schema from "./Schema";

const CERTIFICATE_POLICIES = "certificatePolicies";
const CLEAR_PROPS = [
  CERTIFICATE_POLICIES,
];

export interface ICertificatePolicies {
  certificatePolicies: PolicyInformation[];
}

export type CertificatePoliciesParameters = PkiObjectParameters & Partial<ICertificatePolicies>;

export interface CertificatePoliciesJson {
  certificatePolicies: PolicyInformationJson[];
}

/**
 * Represents the CertificatePolicies structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class CertificatePolicies extends PkiObject implements ICertificatePolicies {

  public static override CLASS_NAME = "CertificatePolicies";

  public certificatePolicies!: PolicyInformation[];

  /**
   * Initializes a new instance of the {@link CertificatePolicies} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CertificatePoliciesParameters = {}) {
    super();

    this.certificatePolicies = pvutils.getParametersValue(parameters, CERTIFICATE_POLICIES, CertificatePolicies.defaultValues(CERTIFICATE_POLICIES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof CERTIFICATE_POLICIES): PolicyInformation[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CERTIFICATE_POLICIES:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{ certificatePolicies?: string; }> = {}) {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.certificatePolicies || EMPTY_STRING),
          value: PolicyInformation.schema()
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
      CertificatePolicies.schema({
        names: {
          certificatePolicies: CERTIFICATE_POLICIES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.certificatePolicies = Array.from(asn1.result.certificatePolicies, element => new PolicyInformation({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    return (new asn1js.Sequence({
      value: Array.from(this.certificatePolicies, o => o.toSchema())
    }));
  }

  public toJSON(): CertificatePoliciesJson {
    return {
      certificatePolicies: Array.from(this.certificatePolicies, o => o.toJSON())
    };
  }

}

