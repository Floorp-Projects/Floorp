import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { IssuerAndSerialNumber, IssuerAndSerialNumberJson } from "./IssuerAndSerialNumber";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const VARIANT = "variant";
const VALUE = "value";
const CLEAR_PROPS = [
  "blockName"
];

export interface IRecipientIdentifier {
  variant: number;
  value: IssuerAndSerialNumber | asn1js.OctetString;
}

export interface RecipientIdentifierJson {
  variant: number;
  value?: IssuerAndSerialNumberJson | asn1js.OctetStringJson;
}

export type RecipientIdentifierParameters = PkiObjectParameters & Partial<IRecipientIdentifier>;

export type RecipientIdentifierSchema = Schema.SchemaParameters;

/**
 * Represents the RecipientIdentifier structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class RecipientIdentifier extends PkiObject implements IRecipientIdentifier {

  public static override CLASS_NAME = "RecipientIdentifier";

  public variant!: number;
  public value!: IssuerAndSerialNumber | asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link RecipientIdentifier} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RecipientIdentifierParameters = {}) {
    super();

    this.variant = pvutils.getParametersValue(parameters, VARIANT, RecipientIdentifier.defaultValues(VARIANT));
    if (VALUE in parameters) {
      this.value = pvutils.getParametersValue(parameters, VALUE, RecipientIdentifier.defaultValues(VALUE));
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
  public static override defaultValues(memberName: typeof VARIANT): number;
  public static override defaultValues(memberName: typeof VALUE): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VARIANT:
        return (-1);
      case VALUE:
        return {};
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
      case VARIANT:
        return (memberValue === (-1));
      case VALUE:
        return (Object.keys(memberValue).length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RecipientIdentifier ::= CHOICE {
   *    issuerAndSerialNumber IssuerAndSerialNumber,
   *    subjectKeyIdentifier [0] SubjectKeyIdentifier }
   *
   * SubjectKeyIdentifier ::= OCTET STRING
   *```
   */
  public static override schema(parameters: RecipientIdentifierSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Choice({
      value: [
        IssuerAndSerialNumber.schema({
          names: {
            blockName: (names.blockName || EMPTY_STRING)
          }
        }),
        new asn1js.Primitive({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
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
      RecipientIdentifier.schema({
        names: {
          blockName: "blockName"
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (asn1.result.blockName.idBlock.tagClass === 1) {
      this.variant = 1;
      this.value = new IssuerAndSerialNumber({ schema: asn1.result.blockName });
    } else {
      this.variant = 2;
      this.value = new asn1js.OctetString({ valueHex: asn1.result.blockName.valueBlock.valueHex });
    }
  }

  public toSchema(): asn1js.BaseBlock<any> {
    // Construct and return new ASN.1 schema for this object
    switch (this.variant) {
      case 1:
        if (!(this.value instanceof IssuerAndSerialNumber)) {
          throw new Error("Incorrect type of RecipientIdentifier.value. It should be IssuerAndSerialNumber.");
        }
        return this.value.toSchema();
      case 2:
        if (!(this.value instanceof asn1js.OctetString)) {
          throw new Error("Incorrect type of RecipientIdentifier.value. It should be ASN.1 OctetString.");
        }
        return new asn1js.Primitive({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          valueHex: this.value.valueBlock.valueHexView
        });
      default:
        return new asn1js.Any() as any;
    }
  }

  public toJSON(): RecipientIdentifierJson {
    const res: RecipientIdentifierJson = {
      variant: this.variant
    };

    if ((this.variant === 1 || this.variant === 2) && this.value) {
      res.value = this.value.toJSON();
    }

    return res;
  }

}

