import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import { Attribute, AttributeJson } from "./Attribute";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const TYPE = "type";
const ATTRIBUTES = "attributes";
const ENCODED_VALUE = "encodedValue";
const CLEAR_PROPS = [
  ATTRIBUTES
];

export interface ISignedAndUnsignedAttributes {
  type: number;
  attributes: Attribute[];
  /**
   * Need to have it in order to successfully process with signature verification
   */
  encodedValue: ArrayBuffer;
}

export interface SignedAndUnsignedAttributesJson {
  type: number;
  attributes: AttributeJson[];
}

export type SignedAndUnsignedAttributesParameters = PkiObjectParameters & Partial<ISignedAndUnsignedAttributes>;

export type SignedAndUnsignedAttributesSchema = Schema.SchemaParameters<{
  tagNumber?: number;
  attributes?: string;
}>;

/**
 * Represents the SignedAndUnsignedAttributes structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class SignedAndUnsignedAttributes extends PkiObject implements ISignedAndUnsignedAttributes {

  public static override CLASS_NAME = "SignedAndUnsignedAttributes";

  public type!: number;
  public attributes!: Attribute[];
  public encodedValue!: ArrayBuffer;

  /**
   * Initializes a new instance of the {@link SignedAndUnsignedAttributes} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SignedAndUnsignedAttributesParameters = {}) {
    super();

    this.type = pvutils.getParametersValue(parameters, TYPE, SignedAndUnsignedAttributes.defaultValues(TYPE));
    this.attributes = pvutils.getParametersValue(parameters, ATTRIBUTES, SignedAndUnsignedAttributes.defaultValues(ATTRIBUTES));
    this.encodedValue = pvutils.getParametersValue(parameters, ENCODED_VALUE, SignedAndUnsignedAttributes.defaultValues(ENCODED_VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof TYPE): number;
  public static override defaultValues(memberName: typeof ATTRIBUTES): Attribute[];
  public static override defaultValues(memberName: typeof ENCODED_VALUE): ArrayBuffer;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TYPE:
        return (-1);
      case ATTRIBUTES:
        return [];
      case ENCODED_VALUE:
        return EMPTY_BUFFER;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compare values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case TYPE:
        return (memberValue === SignedAndUnsignedAttributes.defaultValues(TYPE));
      case ATTRIBUTES:
        return (memberValue.length === 0);
      case ENCODED_VALUE:
        return (memberValue.byteLength === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SignedAttributes ::= SET SIZE (1..MAX) OF Attribute
   *
   * UnsignedAttributes ::= SET SIZE (1..MAX) OF Attribute
   *```
   */
  public static override schema(parameters: SignedAndUnsignedAttributesSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Constructed({
      name: (names.blockName || EMPTY_STRING),
      optional: true,
      idBlock: {
        tagClass: 3, // CONTEXT-SPECIFIC
        tagNumber: names.tagNumber || 0 // "SignedAttributes" = 0, "UnsignedAttributes" = 1
      },
      value: [
        new asn1js.Repeated({
          name: (names.attributes || EMPTY_STRING),
          value: Attribute.schema()
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
      SignedAndUnsignedAttributes.schema({
        names: {
          tagNumber: this.type,
          attributes: ATTRIBUTES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.type = asn1.result.idBlock.tagNumber;
    this.encodedValue = pvtsutils.BufferSourceConverter.toArrayBuffer(asn1.result.valueBeforeDecodeView);

    //#region Change type from "[0]" to "SET" accordingly to standard
    const encodedView = new Uint8Array(this.encodedValue);
    encodedView[0] = 0x31;
    //#endregion

    if ((ATTRIBUTES in asn1.result) === false) {
      if (this.type === 0)
        throw new Error("Wrong structure of SignedUnsignedAttributes");
      else
        return; // Not so important in case of "UnsignedAttributes"
    }

    this.attributes = Array.from(asn1.result.attributes, element => new Attribute({ schema: element }));
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    if (SignedAndUnsignedAttributes.compareWithDefault(TYPE, this.type) || SignedAndUnsignedAttributes.compareWithDefault(ATTRIBUTES, this.attributes))
      throw new Error("Incorrectly initialized \"SignedAndUnsignedAttributes\" class");

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Constructed({
      optional: true,
      idBlock: {
        tagClass: 3, // CONTEXT-SPECIFIC
        tagNumber: this.type // "SignedAttributes" = 0, "UnsignedAttributes" = 1
      },
      value: Array.from(this.attributes, o => o.toSchema())
    }));
    //#endregion
  }

  public toJSON(): SignedAndUnsignedAttributesJson {
    if (SignedAndUnsignedAttributes.compareWithDefault(TYPE, this.type) || SignedAndUnsignedAttributes.compareWithDefault(ATTRIBUTES, this.attributes))
      throw new Error("Incorrectly initialized \"SignedAndUnsignedAttributes\" class");

    return {
      type: this.type,
      attributes: Array.from(this.attributes, o => o.toJSON())
    };
  }

}
