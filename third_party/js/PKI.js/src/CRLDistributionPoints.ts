import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { DistributionPoint, DistributionPointJson } from "./DistributionPoint";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const DISTRIBUTION_POINTS = "distributionPoints";
const CLEAR_PROPS = [
  DISTRIBUTION_POINTS
];

export interface ICRLDistributionPoints {
  distributionPoints: DistributionPoint[];
}

export interface CRLDistributionPointsJson {
  distributionPoints: DistributionPointJson[];
}

export type CRLDistributionPointsParameters = PkiObjectParameters & Partial<ICRLDistributionPoints>;

/**
 * Represents the CRLDistributionPoints structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class CRLDistributionPoints extends PkiObject implements ICRLDistributionPoints {

  public static override CLASS_NAME = "CRLDistributionPoints";

  public distributionPoints!: DistributionPoint[];

  /**
   * Initializes a new instance of the {@link CRLDistributionPoints} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CRLDistributionPointsParameters = {}) {
    super();

    this.distributionPoints = pvutils.getParametersValue(parameters, DISTRIBUTION_POINTS, CRLDistributionPoints.defaultValues(DISTRIBUTION_POINTS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: string): DistributionPoint[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case DISTRIBUTION_POINTS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * CRLDistributionPoints ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    distributionPoints?: string;
  }> = {}): Schema.SchemaType {
    /**
     * @type {Object}
     * @property {string} [blockName]
     * @property {string} [distributionPoints]
     */
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.distributionPoints || EMPTY_STRING),
          value: DistributionPoint.schema()
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
      CRLDistributionPoints.schema({
        names: {
          distributionPoints: DISTRIBUTION_POINTS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.distributionPoints = Array.from(asn1.result.distributionPoints, element => new DistributionPoint({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.distributionPoints, o => o.toSchema())
    }));
  }

  public toJSON(): CRLDistributionPointsJson {
    return {
      distributionPoints: Array.from(this.distributionPoints, o => o.toJSON())
    };
  }

}
