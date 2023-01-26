import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { IssuerAndSerialNumber } from "./IssuerAndSerialNumber";
import { OriginatorPublicKey } from "./OriginatorPublicKey";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const VARIANT = "variant";
const VALUE = "value";
const CLEAR_PROPS = [
  "blockName",
];

export interface IOriginatorIdentifierOrKey {
  variant: number;
  value?: any;
}

export interface OriginatorIdentifierOrKeyJson {
  variant: number;
  value?: any;
}

export type OriginatorIdentifierOrKeyParameters = PkiObjectParameters & Partial<IOriginatorIdentifierOrKey>;

export type OriginatorIdentifierOrKeySchema = Schema.SchemaParameters;

/**
 * Represents the OriginatorIdentifierOrKey structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class OriginatorIdentifierOrKey extends PkiObject implements IOriginatorIdentifierOrKey {

  public static override CLASS_NAME = "OriginatorIdentifierOrKey";

  public variant!: number;
  public value?: any;

  /**
   * Initializes a new instance of the {@link OriginatorIdentifierOrKey} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OriginatorIdentifierOrKeyParameters = {}) {
    super();

    this.variant = pvutils.getParametersValue(parameters, VARIANT, OriginatorIdentifierOrKey.defaultValues(VARIANT));
    if (VALUE in parameters) {
      this.value = pvutils.getParametersValue(parameters, VALUE, OriginatorIdentifierOrKey.defaultValues(VALUE));
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
   * OriginatorIdentifierOrKey ::= CHOICE {
   *    issuerAndSerialNumber IssuerAndSerialNumber,
   *    subjectKeyIdentifier [0] SubjectKeyIdentifier,
   *    originatorKey [1] OriginatorPublicKey }
   *```
   */
  public static override schema(parameters: OriginatorIdentifierOrKeySchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Choice({
      value: [
        IssuerAndSerialNumber.schema({
          names: {
            blockName: (names.blockName || EMPTY_STRING)
          }
        }),
        new asn1js.Primitive({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          name: (names.blockName || EMPTY_STRING)
        }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          name: (names.blockName || EMPTY_STRING),
          value: OriginatorPublicKey.schema().valueBlock.value
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
      OriginatorIdentifierOrKey.schema({
        names: {
          blockName: "blockName"
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    if (asn1.result.blockName.idBlock.tagClass === 1) {
      this.variant = 1;
      this.value = new IssuerAndSerialNumber({ schema: asn1.result.blockName });
    }
    else {
      if (asn1.result.blockName.idBlock.tagNumber === 0) {
        //#region Create "OCTETSTRING" from "ASN1_PRIMITIVE"
        asn1.result.blockName.idBlock.tagClass = 1; // UNIVERSAL
        asn1.result.blockName.idBlock.tagNumber = 4; // OCTETSTRING
        //#endregion

        this.variant = 2;
        this.value = asn1.result.blockName;
      }
      else {
        this.variant = 3;
        this.value = new OriginatorPublicKey({
          schema: new asn1js.Sequence({
            value: asn1.result.blockName.valueBlock.value
          })
        });
      }
    }
    //#endregion
  }

  public toSchema(): asn1js.BaseBlock<any> {
    //#region Construct and return new ASN.1 schema for this object
    switch (this.variant) {
      case 1:
        return this.value.toSchema();
      case 2:
        this.value.idBlock.tagClass = 3; // CONTEXT-SPECIFIC
        this.value.idBlock.tagNumber = 0; // [0]

        return this.value;
      case 3:
        {
          const _schema = this.value.toSchema();

          _schema.idBlock.tagClass = 3; // CONTEXT-SPECIFIC
          _schema.idBlock.tagNumber = 1; // [1]

          return _schema;
        }
      default:
        return new asn1js.Any() as any;
    }
    //#endregion
  }

  public toJSON(): OriginatorIdentifierOrKeyJson {
    const res: OriginatorIdentifierOrKeyJson = {
      variant: this.variant
    };

    if ((this.variant === 1) || (this.variant === 2) || (this.variant === 3)) {
      res.value = this.value.toJSON();
    }

    return res;
  }

}
