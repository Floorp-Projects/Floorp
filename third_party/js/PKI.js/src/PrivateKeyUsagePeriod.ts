import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const NOT_BEFORE = "notBefore";
const NOT_AFTER = "notAfter";
const CLEAR_PROPS = [
  NOT_BEFORE,
  NOT_AFTER
];

export interface IPrivateKeyUsagePeriod {
  notBefore?: Date;
  notAfter?: Date;
}

export interface PrivateKeyUsagePeriodJson {
  notBefore?: Date;
  notAfter?: Date;
}

export type PrivateKeyUsagePeriodParameters = PkiObjectParameters & Partial<IPrivateKeyUsagePeriod>;

/**
 * Represents the PrivateKeyUsagePeriod structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class PrivateKeyUsagePeriod extends PkiObject implements IPrivateKeyUsagePeriod {

  public static override CLASS_NAME = "PrivateKeyUsagePeriod";

  public notBefore?: Date;
  public notAfter?: Date;

  /**
   * Initializes a new instance of the {@link PrivateKeyUsagePeriod} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PrivateKeyUsagePeriodParameters = {}) {
    super();

    if (NOT_BEFORE in parameters) {
      this.notBefore = pvutils.getParametersValue(parameters, NOT_BEFORE, PrivateKeyUsagePeriod.defaultValues(NOT_BEFORE));
    }

    if (NOT_AFTER in parameters) {
      this.notAfter = pvutils.getParametersValue(parameters, NOT_AFTER, PrivateKeyUsagePeriod.defaultValues(NOT_AFTER));
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
  public static override defaultValues(memberName: typeof NOT_BEFORE): Date;
  public static override defaultValues(memberName: typeof NOT_AFTER): Date;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case NOT_BEFORE:
        return new Date();
      case NOT_AFTER:
        return new Date();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PrivateKeyUsagePeriod OID ::= 2.5.29.16
   *
   * PrivateKeyUsagePeriod ::= SEQUENCE {
   *    notBefore       [0]     GeneralizedTime OPTIONAL,
   *    notAfter        [1]     GeneralizedTime OPTIONAL }
   * -- either notBefore or notAfter MUST be present
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    notBefore?: string;
    notAfter?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Primitive({
          name: (names.notBefore || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          }
        }),
        new asn1js.Primitive({
          name: (names.notAfter || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          }
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
      PrivateKeyUsagePeriod.schema({
        names: {
          notBefore: NOT_BEFORE,
          notAfter: NOT_AFTER
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (NOT_BEFORE in asn1.result) {
      const localNotBefore = new asn1js.GeneralizedTime();
      localNotBefore.fromBuffer(asn1.result.notBefore.valueBlock.valueHex);
      this.notBefore = localNotBefore.toDate();
    }
    if (NOT_AFTER in asn1.result) {
      const localNotAfter = new asn1js.GeneralizedTime({ valueHex: asn1.result.notAfter.valueBlock.valueHex });
      localNotAfter.fromBuffer(asn1.result.notAfter.valueBlock.valueHex);
      this.notAfter = localNotAfter.toDate();
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (NOT_BEFORE in this) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        valueHex: (new asn1js.GeneralizedTime({ valueDate: this.notBefore })).valueBlock.valueHexView
      }));
    }

    if (NOT_AFTER in this) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        valueHex: (new asn1js.GeneralizedTime({ valueDate: this.notAfter })).valueBlock.valueHexView
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): PrivateKeyUsagePeriodJson {
    const res: PrivateKeyUsagePeriodJson = {};

    if (this.notBefore) {
      res.notBefore = this.notBefore;
    }

    if (this.notAfter) {
      res.notAfter = this.notAfter;
    }

    return res;
  }

}
