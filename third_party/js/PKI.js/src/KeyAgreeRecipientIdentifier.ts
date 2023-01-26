import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { IssuerAndSerialNumber, IssuerAndSerialNumberJson, IssuerAndSerialNumberSchema } from "./IssuerAndSerialNumber";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RecipientKeyIdentifier, RecipientKeyIdentifierJson, RecipientKeyIdentifierSchema } from "./RecipientKeyIdentifier";
import * as Schema from "./Schema";

const VARIANT = "variant";
const VALUE = "value";
const CLEAR_PROPS = [
  "blockName",
];

export interface IKeyAgreeRecipientIdentifier {
  variant: number;
  value: any;
}

export interface KeyAgreeRecipientIdentifierJson {
  variant: number;
  value?: IssuerAndSerialNumberJson | RecipientKeyIdentifierJson;
}

export type KeyAgreeRecipientIdentifierParameters = PkiObjectParameters & Partial<IKeyAgreeRecipientIdentifier>;

export type KeyAgreeRecipientIdentifierSchema = Schema.SchemaParameters<{
  issuerAndSerialNumber?: IssuerAndSerialNumberSchema;
  rKeyId?: RecipientKeyIdentifierSchema;
}>;

/**
 * Represents the KeyAgreeRecipientIdentifier structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class KeyAgreeRecipientIdentifier extends PkiObject implements IKeyAgreeRecipientIdentifier {

  public static override CLASS_NAME = "KeyAgreeRecipientIdentifier";

  public variant!: number;
  public value: any;

  /**
   * Initializes a new instance of the {@link KeyAgreeRecipientIdentifier} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: KeyAgreeRecipientIdentifierParameters = {}) {
    super();

    this.variant = pvutils.getParametersValue(parameters, VARIANT, KeyAgreeRecipientIdentifier.defaultValues(VARIANT));
    this.value = pvutils.getParametersValue(parameters, VALUE, KeyAgreeRecipientIdentifier.defaultValues(VALUE));

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
   * KeyAgreeRecipientIdentifier ::= CHOICE {
   *    issuerAndSerialNumber IssuerAndSerialNumber,
   *    rKeyId [0] IMPLICIT RecipientKeyIdentifier }
   *```
   */
  public static override schema(parameters: KeyAgreeRecipientIdentifierSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Choice({
      value: [
        IssuerAndSerialNumber.schema(names.issuerAndSerialNumber || {
          names: {
            blockName: (names.blockName || EMPTY_STRING)
          }
        }),
        new asn1js.Constructed({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: RecipientKeyIdentifier.schema(names.rKeyId || {
            names: {
              blockName: (names.blockName || EMPTY_STRING)
            }
          }).valueBlock.value
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
      KeyAgreeRecipientIdentifier.schema({
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

      this.value = new RecipientKeyIdentifier({
        schema: new asn1js.Sequence({
          value: asn1.result.blockName.valueBlock.value
        })
      });
    }
  }

  public toSchema(): asn1js.BaseBlock<any> {
    //#region Construct and return new ASN.1 schema for this object
    switch (this.variant) {
      case 1:
        return this.value.toSchema();
      case 2:
        return new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: this.value.toSchema().valueBlock.value
        });
      default:
        return new asn1js.Any() as any;
    }
    //#endregion
  }

  public toJSON(): KeyAgreeRecipientIdentifierJson {
    const res: KeyAgreeRecipientIdentifierJson = {
      variant: this.variant,
    };

    if ((this.variant === 1) || (this.variant === 2)) {
      res.value = this.value.toJSON();
    }

    return res;
  }

}
