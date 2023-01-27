import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { KeyAgreeRecipientIdentifier, KeyAgreeRecipientIdentifierJson, KeyAgreeRecipientIdentifierSchema } from "./KeyAgreeRecipientIdentifier";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const RID = "rid";
const ENCRYPTED_KEY = "encryptedKey";
const CLEAR_PROPS = [
  RID,
  ENCRYPTED_KEY,
];

export interface IRecipientEncryptedKey {
  rid: KeyAgreeRecipientIdentifier;
  encryptedKey: asn1js.OctetString;
}

export interface RecipientEncryptedKeyJson {
  rid: KeyAgreeRecipientIdentifierJson;
  encryptedKey: asn1js.OctetStringJson;
}

export type RecipientEncryptedKeyParameters = PkiObjectParameters & Partial<IRecipientEncryptedKey>;

/**
 * Represents the RecipientEncryptedKey structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class RecipientEncryptedKey extends PkiObject implements IRecipientEncryptedKey {

  public static override CLASS_NAME = "RecipientEncryptedKey";

  public rid!: KeyAgreeRecipientIdentifier;
  public encryptedKey!: asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link RecipientEncryptedKey} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RecipientEncryptedKeyParameters = {}) {
    super();

    this.rid = pvutils.getParametersValue(parameters, RID, RecipientEncryptedKey.defaultValues(RID));
    this.encryptedKey = pvutils.getParametersValue(parameters, ENCRYPTED_KEY, RecipientEncryptedKey.defaultValues(ENCRYPTED_KEY));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof RID): KeyAgreeRecipientIdentifier;
  public static override defaultValues(memberName: typeof ENCRYPTED_KEY): asn1js.OctetString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case RID:
        return new KeyAgreeRecipientIdentifier();
      case ENCRYPTED_KEY:
        return new asn1js.OctetString();
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
      case RID:
        return ((memberValue.variant === (-1)) && (("value" in memberValue) === false));
      case ENCRYPTED_KEY:
        return (memberValue.isEqual(RecipientEncryptedKey.defaultValues(ENCRYPTED_KEY)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RecipientEncryptedKey ::= SEQUENCE {
   *    rid KeyAgreeRecipientIdentifier,
   *    encryptedKey EncryptedKey }
   *
   * EncryptedKey ::= OCTET STRING
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    rid?: KeyAgreeRecipientIdentifierSchema;
    encryptedKey?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        KeyAgreeRecipientIdentifier.schema(names.rid || {}),
        new asn1js.OctetString({ name: (names.encryptedKey || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      RecipientEncryptedKey.schema({
        names: {
          rid: {
            names: {
              blockName: RID
            }
          },
          encryptedKey: ENCRYPTED_KEY
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.rid = new KeyAgreeRecipientIdentifier({ schema: asn1.result.rid });
    this.encryptedKey = asn1.result.encryptedKey;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        this.rid.toSchema(),
        this.encryptedKey
      ]
    }));
    //#endregion
  }

  public toJSON(): RecipientEncryptedKeyJson {
    return {
      rid: this.rid.toJSON(),
      encryptedKey: this.encryptedKey.toJSON(),
    };
  }

}
