import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { stringPrep } from "./Helpers";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const TYPE = "type";
const VALUE = "value";

export interface IAttributeTypeAndValue {
  type: string;
  value: AttributeValueType;
}

export type AttributeTypeAndValueParameters = PkiObjectParameters & Partial<IAttributeTypeAndValue>;

export type AttributeValueType = asn1js.Utf8String
  | asn1js.BmpString
  | asn1js.UniversalString
  | asn1js.NumericString
  | asn1js.PrintableString
  | asn1js.TeletexString
  | asn1js.VideotexString
  | asn1js.IA5String
  | asn1js.GraphicString
  | asn1js.VisibleString
  | asn1js.GeneralString
  | asn1js.CharacterString;

export interface AttributeTypeAndValueJson {
  type: string;
  value: any;
}

/**
 * Represents the AttributeTypeAndValue structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class AttributeTypeAndValue extends PkiObject implements IAttributeTypeAndValue {

  public static override CLASS_NAME = "AttributeTypeAndValue";

  public type!: string;
  public value!: AttributeValueType;

  /**
   * Initializes a new instance of the {@link AttributeTypeAndValue} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AttributeTypeAndValueParameters = {}) {
    super();

    this.type = pvutils.getParametersValue(parameters, TYPE, AttributeTypeAndValue.defaultValues(TYPE));
    this.value = pvutils.getParametersValue(parameters, VALUE, AttributeTypeAndValue.defaultValues(VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TYPE): string;
  public static override defaultValues(memberName: typeof VALUE): AttributeValueType;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TYPE:
        return EMPTY_STRING;
      case VALUE:
        return {};
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AttributeTypeAndValue ::= Sequence {
   *    type     AttributeType,
   *    value    AttributeValue }
   *
   * AttributeType ::= OBJECT IDENTIFIER
   *
   * AttributeValue ::= ANY -- DEFINED BY AttributeType
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{ type?: string, value?: string; }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.type || EMPTY_STRING) }),
        new asn1js.Any({ name: (names.value || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType) {
    //#region Clear input data first
    pvutils.clearProps(schema, [
      TYPE,
      "typeValue"
    ]);
    //#endregion

    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      AttributeTypeAndValue.schema({
        names: {
          type: TYPE,
          value: "typeValue"
        }
      })
    );

    AsnError.assertSchema(asn1, this.className);
    //#endregion

    //#region Get internal properties from parsed schema
    this.type = asn1.result.type.valueBlock.toString();
    this.value = asn1.result.typeValue;
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.type }),
        this.value
      ]
    }));
    //#endregion
  }

  public toJSON(): AttributeTypeAndValueJson {
    const _object = {
      type: this.type
    } as AttributeTypeAndValueJson;

    if (Object.keys(this.value).length !== 0) {
      _object.value = (this.value).toJSON();
    } else {
      _object.value = this.value;
    }

    return _object;
  }

  /**
   * Compares two AttributeTypeAndValue values, or AttributeTypeAndValue with ArrayBuffer value
   * @param compareTo The value compare to current
   */
  public isEqual(compareTo: AttributeTypeAndValue | ArrayBuffer): boolean {
    const stringBlockNames = [
      asn1js.Utf8String.blockName(),
      asn1js.BmpString.blockName(),
      asn1js.UniversalString.blockName(),
      asn1js.NumericString.blockName(),
      asn1js.PrintableString.blockName(),
      asn1js.TeletexString.blockName(),
      asn1js.VideotexString.blockName(),
      asn1js.IA5String.blockName(),
      asn1js.GraphicString.blockName(),
      asn1js.VisibleString.blockName(),
      asn1js.GeneralString.blockName(),
      asn1js.CharacterString.blockName()
    ];

    if (compareTo instanceof ArrayBuffer) {
      return pvtsutils.BufferSourceConverter.isEqual(this.value.valueBeforeDecodeView, compareTo);
    }

    if ((compareTo.constructor as typeof AttributeTypeAndValue).blockName() === AttributeTypeAndValue.blockName()) {
      if (this.type !== compareTo.type)
        return false;

      //#region Check we do have both strings
      const isStringPair = [false, false];
      const thisName = (this.value.constructor as typeof asn1js.BaseBlock).blockName();
      for (const name of stringBlockNames) {
        if (thisName === name) {
          isStringPair[0] = true;
        }
        if ((compareTo.value.constructor as typeof asn1js.BaseBlock).blockName() === name) {
          isStringPair[1] = true;
        }
      }

      if (isStringPair[0] !== isStringPair[1]) {
        return false;
      }

      const isString = (isStringPair[0] && isStringPair[1]);
      //#endregion

      if (isString) {
        const value1 = stringPrep(this.value.valueBlock.value);
        const value2 = stringPrep(compareTo.value.valueBlock.value);

        if (value1.localeCompare(value2) !== 0)
          return false;
      }
      else // Comparing as two ArrayBuffers
      {
        if (!pvtsutils.BufferSourceConverter.isEqual(this.value.valueBeforeDecodeView, compareTo.value.valueBeforeDecodeView))
          return false;
      }

      return true;
    }

    return false;
  }

}
