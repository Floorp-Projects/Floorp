import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "../constants";
import { AsnError } from "../errors";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import * as Schema from "../Schema";

const NOT_BEFORE_TIME = "notBeforeTime";
const NOT_AFTER_TIME = "notAfterTime";
const CLEAR_PROPS = [
  NOT_BEFORE_TIME,
  NOT_AFTER_TIME,
];

export interface IAttCertValidityPeriod {
  notBeforeTime: Date;
  notAfterTime: Date;
}

export type AttCertValidityPeriodParameters = PkiObjectParameters & Partial<IAttCertValidityPeriod>;

export type AttCertValidityPeriodSchema = Schema.SchemaParameters<{
  notBeforeTime?: string;
  notAfterTime?: string;
}>;

export interface AttCertValidityPeriodJson {
  notBeforeTime: Date;
  notAfterTime: Date;
}

/**
 * Represents the AttCertValidityPeriod structure described in [RFC5755 Section 4.1](https://datatracker.ietf.org/doc/html/rfc5755#section-4.1)
 */
export class AttCertValidityPeriod extends PkiObject implements IAttCertValidityPeriod {

  public static override CLASS_NAME = "AttCertValidityPeriod";

  public notBeforeTime!: Date;
  public notAfterTime!: Date;

  /**
   * Initializes a new instance of the {@link AttCertValidityPeriod} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AttCertValidityPeriodParameters = {}) {
    super();

    this.notBeforeTime = pvutils.getParametersValue(parameters, NOT_BEFORE_TIME, AttCertValidityPeriod.defaultValues(NOT_BEFORE_TIME));
    this.notAfterTime = pvutils.getParametersValue(parameters, NOT_AFTER_TIME, AttCertValidityPeriod.defaultValues(NOT_AFTER_TIME));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof NOT_BEFORE_TIME): Date;
  public static override defaultValues(memberName: typeof NOT_AFTER_TIME): Date;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case NOT_BEFORE_TIME:
      case NOT_AFTER_TIME:
        return new Date(0, 0, 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AttCertValidityPeriod ::= SEQUENCE {
   *   notBeforeTime  GeneralizedTime,
   *   notAfterTime   GeneralizedTime
   * }
   *```
   */
  public static override schema(parameters: AttCertValidityPeriodSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.GeneralizedTime({ name: (names.notBeforeTime || EMPTY_STRING) }),
        new asn1js.GeneralizedTime({ name: (names.notAfterTime || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      AttCertValidityPeriod.schema({
        names: {
          notBeforeTime: NOT_BEFORE_TIME,
          notAfterTime: NOT_AFTER_TIME
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.notBeforeTime = asn1.result.notBeforeTime.toDate();
    this.notAfterTime = asn1.result.notAfterTime.toDate();
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.GeneralizedTime({ valueDate: this.notBeforeTime }),
        new asn1js.GeneralizedTime({ valueDate: this.notAfterTime }),
      ]
    }));
  }

  public toJSON(): AttCertValidityPeriodJson {
    return {
      notBeforeTime: this.notBeforeTime,
      notAfterTime: this.notAfterTime
    };
  }

}
