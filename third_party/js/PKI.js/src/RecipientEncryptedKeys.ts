import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RecipientEncryptedKey, RecipientEncryptedKeyJson } from "./RecipientEncryptedKey";
import * as Schema from "./Schema";

const ENCRYPTED_KEYS = "encryptedKeys";
const RECIPIENT_ENCRYPTED_KEYS = "RecipientEncryptedKeys";
const CLEAR_PROPS = [
  RECIPIENT_ENCRYPTED_KEYS,
];

export interface IRecipientEncryptedKeys {
  encryptedKeys: RecipientEncryptedKey[];
}

export interface RecipientEncryptedKeysJson {
  encryptedKeys: RecipientEncryptedKeyJson[];
}

export type RecipientEncryptedKeysParameters = PkiObjectParameters & Partial<IRecipientEncryptedKeys>;

export type RecipientEncryptedKeysSchema = Schema.SchemaParameters<{
  RecipientEncryptedKeys?: string;
}>;

/**
 * Represents the RecipientEncryptedKeys structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class RecipientEncryptedKeys extends PkiObject implements IRecipientEncryptedKeys {

  public static override CLASS_NAME = "RecipientEncryptedKeys";

  public encryptedKeys!: RecipientEncryptedKey[];

  /**
   * Initializes a new instance of the {@link RecipientEncryptedKeys} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RecipientEncryptedKeysParameters = {}) {
    super();

    this.encryptedKeys = pvutils.getParametersValue(parameters, ENCRYPTED_KEYS, RecipientEncryptedKeys.defaultValues(ENCRYPTED_KEYS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ENCRYPTED_KEYS): RecipientEncryptedKey[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ENCRYPTED_KEYS:
        return [];
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
      case ENCRYPTED_KEYS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RecipientEncryptedKeys ::= SEQUENCE OF RecipientEncryptedKey
   *```
   */
  public static override schema(parameters: RecipientEncryptedKeysSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.RecipientEncryptedKeys || EMPTY_STRING),
          value: RecipientEncryptedKey.schema()
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
      RecipientEncryptedKeys.schema({
        names: {
          RecipientEncryptedKeys: RECIPIENT_ENCRYPTED_KEYS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.encryptedKeys = Array.from(asn1.result.RecipientEncryptedKeys, element => new RecipientEncryptedKey({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.encryptedKeys, o => o.toSchema())
    }));
  }

  public toJSON(): RecipientEncryptedKeysJson {
    return {
      encryptedKeys: Array.from(this.encryptedKeys, o => o.toJSON())
    };
  }

}
