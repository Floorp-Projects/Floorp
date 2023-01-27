import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AttributeTypeAndValue, AttributeTypeAndValueJson } from "./AttributeTypeAndValue";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

export const TYPE_AND_VALUES = "typesAndValues";
export const VALUE_BEFORE_DECODE = "valueBeforeDecode";
export const RDN = "RDN";

export interface IRelativeDistinguishedNames {
  /**
   * Array of "type and value" objects
   */
  typesAndValues: AttributeTypeAndValue[];
  /**
   * Value of the RDN before decoding from schema
   */
  valueBeforeDecode: ArrayBuffer;
}

export type RelativeDistinguishedNamesParameters = PkiObjectParameters & Partial<IRelativeDistinguishedNames>;

export type RelativeDistinguishedNamesSchema = Schema.SchemaParameters<{
  repeatedSequence?: string;
  repeatedSet?: string;
  typeAndValue?: Schema.SchemaType;
}>;

export interface RelativeDistinguishedNamesJson {
  typesAndValues: AttributeTypeAndValueJson[];
}

/**
 * Represents the RelativeDistinguishedNames structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class RelativeDistinguishedNames extends PkiObject implements IRelativeDistinguishedNames {

  public static override CLASS_NAME = "RelativeDistinguishedNames";

  public typesAndValues!: AttributeTypeAndValue[];
  public valueBeforeDecode!: ArrayBuffer;

  /**
   * Initializes a new instance of the {@link RelativeDistinguishedNames} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RelativeDistinguishedNamesParameters = {}) {
    super();

    this.typesAndValues = pvutils.getParametersValue(parameters, TYPE_AND_VALUES, RelativeDistinguishedNames.defaultValues(TYPE_AND_VALUES));
    this.valueBeforeDecode = pvutils.getParametersValue(parameters, VALUE_BEFORE_DECODE, RelativeDistinguishedNames.defaultValues(VALUE_BEFORE_DECODE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TYPE_AND_VALUES): AttributeTypeAndValue[];
  public static override defaultValues(memberName: typeof VALUE_BEFORE_DECODE): ArrayBuffer;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TYPE_AND_VALUES:
        return [];
      case VALUE_BEFORE_DECODE:
        return EMPTY_BUFFER;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compares values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case TYPE_AND_VALUES:
        return (memberValue.length === 0);
      case VALUE_BEFORE_DECODE:
        return (memberValue.byteLength === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RDNSequence ::= Sequence OF RelativeDistinguishedName
   *
   * RelativeDistinguishedName ::=
   * SET SIZE (1..MAX) OF AttributeTypeAndValue
   *```
   */
  static override schema(parameters: RelativeDistinguishedNamesSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.repeatedSequence || EMPTY_STRING),
          value: new asn1js.Set({
            value: [
              new asn1js.Repeated({
                name: (names.repeatedSet || EMPTY_STRING),
                value: AttributeTypeAndValue.schema(names.typeAndValue || {})
              })
            ]
          } as any)
        } as any)
      ]
    } as any));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, [
      RDN,
      TYPE_AND_VALUES
    ]);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      RelativeDistinguishedNames.schema({
        names: {
          blockName: RDN,
          repeatedSet: TYPE_AND_VALUES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (TYPE_AND_VALUES in asn1.result) {// Could be a case when there is no "types and values"
      this.typesAndValues = Array.from(asn1.result.typesAndValues, element => new AttributeTypeAndValue({ schema: element }));
    }

    this.valueBeforeDecode = (asn1.result.RDN as asn1js.BaseBlock).valueBeforeDecodeView.slice().buffer;
  }

  public toSchema(): asn1js.Sequence {
    if (this.valueBeforeDecode.byteLength === 0) // No stored encoded array, create "from scratch"
    {
      return (new asn1js.Sequence({
        value: [new asn1js.Set({
          value: Array.from(this.typesAndValues, o => o.toSchema())
        } as any)]
      } as any));
    }

    const asn1 = asn1js.fromBER(this.valueBeforeDecode);
    AsnError.assert(asn1, "RelativeDistinguishedNames");
    if (!(asn1.result instanceof asn1js.Sequence)) {
      throw new Error("ASN.1 result should be SEQUENCE");
    }

    return asn1.result;
  }

  public toJSON(): RelativeDistinguishedNamesJson {
    return {
      typesAndValues: Array.from(this.typesAndValues, o => o.toJSON())
    };
  }

  /**
   * Compares two RDN values, or RDN with ArrayBuffer value
   * @param compareTo The value compare to current
   */
  public isEqual(compareTo: unknown): boolean {
    if (compareTo instanceof RelativeDistinguishedNames) {
      if (this.typesAndValues.length !== compareTo.typesAndValues.length)
        return false;

      for (const [index, typeAndValue] of this.typesAndValues.entries()) {
        if (typeAndValue.isEqual(compareTo.typesAndValues[index]) === false)
          return false;
      }

      return true;
    }

    if (compareTo instanceof ArrayBuffer) {
      return pvutils.isEqualBuffer(this.valueBeforeDecode, compareTo);
    }

    return false;
  }

}
