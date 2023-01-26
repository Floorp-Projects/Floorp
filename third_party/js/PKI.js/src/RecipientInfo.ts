import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { KeyTransRecipientInfo, KeyTransRecipientInfoJson } from "./KeyTransRecipientInfo";
import { KeyAgreeRecipientInfo, KeyAgreeRecipientInfoJson } from "./KeyAgreeRecipientInfo";
import { KEKRecipientInfo, KEKRecipientInfoJson } from "./KEKRecipientInfo";
import { PasswordRecipientinfo, PasswordRecipientInfoJson } from "./PasswordRecipientinfo";
import { OtherRecipientInfo, OtherRecipientInfoJson } from "./OtherRecipientInfo";
import * as Schema from "./Schema";
import { AsnError, ParameterError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_STRING } from "./constants";

const VARIANT = "variant";
const VALUE = "value";
const CLEAR_PROPS = [
  "blockName"
];

export interface IRecipientInfo {
  variant: number;
  value?: RecipientInfoValue;
}

export interface RecipientInfoJson {
  variant: number;
  value?: RecipientInfoValueJson;
}

export type RecipientInfoValue = KeyTransRecipientInfo | KeyAgreeRecipientInfo | KEKRecipientInfo | PasswordRecipientinfo | OtherRecipientInfo;
export type RecipientInfoValueJson = KeyTransRecipientInfoJson | KeyAgreeRecipientInfoJson | KEKRecipientInfoJson | PasswordRecipientInfoJson | OtherRecipientInfoJson;

export type RecipientInfoParameters = PkiObjectParameters & Partial<IRecipientInfo>;

/**
 * Represents the RecipientInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class RecipientInfo extends PkiObject implements IRecipientInfo {

  public static override CLASS_NAME = "RecipientInfo";

  public variant!: number;
  public value?: RecipientInfoValue;

  /**
   * Initializes a new instance of the {@link RecipientInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RecipientInfoParameters = {}) {
    super();

    this.variant = pvutils.getParametersValue(parameters, VARIANT, RecipientInfo.defaultValues(VARIANT));
    if (VALUE in parameters) {
      this.value = pvutils.getParametersValue(parameters, VALUE, RecipientInfo.defaultValues(VALUE));
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
  public static override defaultValues(memberName: typeof VALUE): RecipientInfoValue;
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
        return (memberValue === RecipientInfo.defaultValues(memberName));
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
   * RecipientInfo ::= CHOICE {
   *    ktri KeyTransRecipientInfo,
   *    kari [1] KeyAgreeRecipientInfo,
   *    kekri [2] KEKRecipientInfo,
   *    pwri [3] PasswordRecipientinfo,
   *    ori [4] OtherRecipientInfo }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Choice({
      value: [
        KeyTransRecipientInfo.schema({
          names: {
            blockName: (names.blockName || EMPTY_STRING)
          }
        }),
        new asn1js.Constructed({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: KeyAgreeRecipientInfo.schema().valueBlock.value
        }),
        new asn1js.Constructed({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          },
          value: KEKRecipientInfo.schema().valueBlock.value
        }),
        new asn1js.Constructed({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 3 // [3]
          },
          value: PasswordRecipientinfo.schema().valueBlock.value
        }),
        new asn1js.Constructed({
          name: (names.blockName || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 4 // [4]
          },
          value: OtherRecipientInfo.schema().valueBlock.value
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
      RecipientInfo.schema({
        names: {
          blockName: "blockName"
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (asn1.result.blockName.idBlock.tagClass === 1) {
      this.variant = 1;
      this.value = new KeyTransRecipientInfo({ schema: asn1.result.blockName });
    } else {
      // Create "SEQUENCE" from "ASN1_CONSTRUCTED"
      const blockSequence = new asn1js.Sequence({
        value: asn1.result.blockName.valueBlock.value
      });

      switch (asn1.result.blockName.idBlock.tagNumber) {
        case 1:
          this.variant = 2;
          this.value = new KeyAgreeRecipientInfo({ schema: blockSequence });
          break;
        case 2:
          this.variant = 3;
          this.value = new KEKRecipientInfo({ schema: blockSequence });
          break;
        case 3:
          this.variant = 4;
          this.value = new PasswordRecipientinfo({ schema: blockSequence });
          break;
        case 4:
          this.variant = 5;
          this.value = new OtherRecipientInfo({ schema: blockSequence });
          break;
        default:
          throw new Error("Incorrect structure of RecipientInfo block");
      }
    }
  }

  public toSchema(): asn1js.BaseBlock<any> {
    // Construct and return new ASN.1 schema for this object
    ParameterError.assertEmpty(this.value, "value", "RecipientInfo");
    const _schema = this.value.toSchema();

    switch (this.variant) {
      case 1:
        return _schema;
      case 2:
      case 3:
      case 4:
        // Create "ASN1_CONSTRUCTED" from "SEQUENCE"
        _schema.idBlock.tagClass = 3; // CONTEXT-SPECIFIC
        _schema.idBlock.tagNumber = (this.variant - 1);

        return _schema;
      default:
        return new asn1js.Any() as any;
    }
  }

  public toJSON(): RecipientInfoJson {
    const res: RecipientInfoJson = {
      variant: this.variant
    };

    if (this.value && (this.variant >= 1) && (this.variant <= 4)) {
      res.value = this.value.toJSON();
    }

    return res;
  }

}
