import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

export const SECONDS = "seconds";
export const MILLIS = "millis";
export const MICROS = "micros";

export interface IAccuracy {
  /**
   * Seconds
   */
  seconds?: number;
  /**
   * Milliseconds
   */
  millis?: number;
  /**
   * Microseconds
   */
  micros?: number;
}

export type AccuracyParameters = PkiObjectParameters & Partial<IAccuracy>;

export type AccuracySchema = Schema.SchemaParameters<{
  seconds?: string;
  millis?: string;
  micros?: string;
}>;

/**
 * JSON representation of {@link Accuracy}
 */
export interface AccuracyJson {
  seconds?: number;
  millis?: number;
  micros?: number;
}

/**
 * Represents the time deviation around the UTC time contained in GeneralizedTime. Described in [RFC3161](https://www.ietf.org/rfc/rfc3161.txt)
 */
export class Accuracy extends PkiObject implements IAccuracy {

  public static override CLASS_NAME = "Accuracy";

  public seconds?: number;
  public millis?: number;
  public micros?: number;

  /**
   * Initializes a new instance of the {@link Accuracy} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AccuracyParameters = {}) {
    super();

    if (SECONDS in parameters) {
      this.seconds = pvutils.getParametersValue(parameters, SECONDS, Accuracy.defaultValues(SECONDS));
    }
    if (MILLIS in parameters) {
      this.millis = pvutils.getParametersValue(parameters, MILLIS, Accuracy.defaultValues(MILLIS));
    }
    if (MICROS in parameters) {
      this.micros = pvutils.getParametersValue(parameters, MICROS, Accuracy.defaultValues(MICROS));
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
  public static override defaultValues(memberName: typeof SECONDS): number;
  public static override defaultValues(memberName: typeof MILLIS): number;
  public static override defaultValues(memberName: typeof MICROS): number;
  public static override defaultValues(memberName: string): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case SECONDS:
      case MILLIS:
      case MICROS:
        return 0;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compare values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault(memberName: typeof SECONDS | typeof MILLIS | typeof MICROS, memberValue: number): boolean;
  public static compareWithDefault(memberName: string, memberValue: any): boolean;
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case SECONDS:
      case MILLIS:
      case MICROS:
        return (memberValue === Accuracy.defaultValues(memberName));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * Accuracy ::= SEQUENCE {
   *    seconds        INTEGER              OPTIONAL,
   *    millis     [0] INTEGER  (1..999)    OPTIONAL,
   *    micros     [1] INTEGER  (1..999)    OPTIONAL  }
   *```
   */
  public static override schema(parameters: AccuracySchema = {}): any {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      optional: true,
      value: [
        new asn1js.Integer({
          optional: true,
          name: (names.seconds || EMPTY_STRING)
        }),
        new asn1js.Primitive({
          name: (names.millis || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          }
        }),
        new asn1js.Primitive({
          name: (names.micros || EMPTY_STRING),
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
    pvutils.clearProps(schema, [
      SECONDS,
      MILLIS,
      MICROS,
    ]);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      Accuracy.schema({
        names: {
          seconds: SECONDS,
          millis: MILLIS,
          micros: MICROS,
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if ("seconds" in asn1.result) {
      this.seconds = asn1.result.seconds.valueBlock.valueDec;
    }
    if ("millis" in asn1.result) {
      const intMillis = new asn1js.Integer({ valueHex: asn1.result.millis.valueBlock.valueHex });
      this.millis = intMillis.valueBlock.valueDec;
    }
    if ("micros" in asn1.result) {
      const intMicros = new asn1js.Integer({ valueHex: asn1.result.micros.valueBlock.valueHex });
      this.micros = intMicros.valueBlock.valueDec;
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array of output sequence
    const outputArray = [];

    if (this.seconds !== undefined)
      outputArray.push(new asn1js.Integer({ value: this.seconds }));

    if (this.millis !== undefined) {
      const intMillis = new asn1js.Integer({ value: this.millis });

      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        valueHex: intMillis.valueBlock.valueHexView
      }));
    }

    if (this.micros !== undefined) {
      const intMicros = new asn1js.Integer({ value: this.micros });

      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        valueHex: intMicros.valueBlock.valueHexView
      }));
    }
    //#endregion

    return (new asn1js.Sequence({
      value: outputArray
    }));
  }
  public toJSON(): AccuracyJson {
    const _object: AccuracyJson = {};

    if (this.seconds !== undefined)
      _object.seconds = this.seconds;

    if (this.millis !== undefined)
      _object.millis = this.millis;

    if (this.micros !== undefined)
      _object.micros = this.micros;

    return _object;
  }

}

