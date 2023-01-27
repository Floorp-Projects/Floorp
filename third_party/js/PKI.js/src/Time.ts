import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const TYPE = "type";
const VALUE = "value";
const UTC_TIME_NAME = "utcTimeName";
const GENERAL_TIME_NAME = "generalTimeName";
const CLEAR_PROPS = [UTC_TIME_NAME, GENERAL_TIME_NAME];

export enum TimeType {
  UTCTime,
  GeneralizedTime,
  empty,
}

export interface ITime {
  /**
   * 0 - UTCTime; 1 - GeneralizedTime; 2 - empty value
   */
  type: TimeType;
  /**
   * Value of the TIME class
   */
  value: Date;
}

export type TimeParameters = PkiObjectParameters & Partial<ITime>;

export type TimeSchema = Schema.SchemaParameters<{
  utcTimeName?: string;
  generalTimeName?: string;
}>;

export interface TimeJson {
  type: TimeType;
  value: Date;
}

/**
 * Represents the Time structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class Time extends PkiObject implements ITime {

  public static override CLASS_NAME = "Time";

  public type!: TimeType;
  public value!: Date;

  /**
   * Initializes a new instance of the {@link Time} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: TimeParameters = {}) {
    super();

    this.type = pvutils.getParametersValue(parameters, TYPE, Time.defaultValues(TYPE));
    this.value = pvutils.getParametersValue(parameters, VALUE, Time.defaultValues(VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TYPE): TimeType;
  public static override defaultValues(memberName: typeof VALUE): Date;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TYPE:
        return 0;
      case VALUE:
        return new Date(0, 0, 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * Time ::= CHOICE {
     *   utcTime        UTCTime,
     *   generalTime    GeneralizedTime }
   * ```
   *
   * @param parameters Input parameters for the schema
   * @param optional Flag that current schema should be optional
   * @returns ASN.1 schema object
   */
  static override schema(parameters: TimeSchema = {}, optional = false): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Choice({
      optional,
      value: [
        new asn1js.UTCTime({ name: (names.utcTimeName || EMPTY_STRING) }),
        new asn1js.GeneralizedTime({ name: (names.generalTimeName || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema, schema, Time.schema({
      names: {
        utcTimeName: UTC_TIME_NAME,
        generalTimeName: GENERAL_TIME_NAME
      }
    }));
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (UTC_TIME_NAME in asn1.result) {
      this.type = 0;
      this.value = asn1.result.utcTimeName.toDate();
    }
    if (GENERAL_TIME_NAME in asn1.result) {
      this.type = 1;
      this.value = asn1.result.generalTimeName.toDate();
    }
  }

  public toSchema(): asn1js.UTCTime | asn1js.GeneralizedTime {
    if (this.type === 0) {
      return new asn1js.UTCTime({ valueDate: this.value });
    } else if (this.type === 1) {
      return new asn1js.GeneralizedTime({ valueDate: this.value });
    }

    return {} as any;
  }

  public toJSON(): TimeJson {
    return {
      type: this.type,
      value: this.value
    };
  }

}
