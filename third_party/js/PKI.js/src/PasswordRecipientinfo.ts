import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const VERSION = "version";
const KEY_DERIVATION_ALGORITHM = "keyDerivationAlgorithm";
const KEY_ENCRYPTION_ALGORITHM = "keyEncryptionAlgorithm";
const ENCRYPTED_KEY = "encryptedKey";
const PASSWORD = "password";
const CLEAR_PROPS = [
  VERSION,
  KEY_DERIVATION_ALGORITHM,
  KEY_ENCRYPTION_ALGORITHM,
  ENCRYPTED_KEY
];

export interface IPasswordRecipientInfo {
  version: number;
  keyDerivationAlgorithm?: AlgorithmIdentifier;
  keyEncryptionAlgorithm: AlgorithmIdentifier;
  encryptedKey: asn1js.OctetString;
  password: ArrayBuffer;
}

export interface PasswordRecipientInfoJson {
  version: number;
  keyDerivationAlgorithm?: AlgorithmIdentifierJson;
  keyEncryptionAlgorithm: AlgorithmIdentifierJson;
  encryptedKey: asn1js.OctetStringJson;
}

export type PasswordRecipientinfoParameters = PkiObjectParameters & Partial<IPasswordRecipientInfo>;

/**
 * Represents the PasswordRecipientInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
// TODO rename to PasswordRecipientInfo
export class PasswordRecipientinfo extends PkiObject implements IPasswordRecipientInfo {

  public static override CLASS_NAME = "PasswordRecipientInfo";

  public version!: number;
  public keyDerivationAlgorithm?: AlgorithmIdentifier;
  public keyEncryptionAlgorithm!: AlgorithmIdentifier;
  public encryptedKey!: asn1js.OctetString;
  public password!: ArrayBuffer;

  /**
   * Initializes a new instance of the {@link PasswordRecipientinfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PasswordRecipientinfoParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, PasswordRecipientinfo.defaultValues(VERSION));
    if (KEY_DERIVATION_ALGORITHM in parameters) {
      this.keyDerivationAlgorithm = pvutils.getParametersValue(parameters, KEY_DERIVATION_ALGORITHM, PasswordRecipientinfo.defaultValues(KEY_DERIVATION_ALGORITHM));
    }
    this.keyEncryptionAlgorithm = pvutils.getParametersValue(parameters, KEY_ENCRYPTION_ALGORITHM, PasswordRecipientinfo.defaultValues(KEY_ENCRYPTION_ALGORITHM));
    this.encryptedKey = pvutils.getParametersValue(parameters, ENCRYPTED_KEY, PasswordRecipientinfo.defaultValues(ENCRYPTED_KEY));
    this.password = pvutils.getParametersValue(parameters, PASSWORD, PasswordRecipientinfo.defaultValues(PASSWORD));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof KEY_DERIVATION_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof KEY_ENCRYPTION_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ENCRYPTED_KEY): asn1js.OctetString;
  public static override defaultValues(memberName: typeof PASSWORD): ArrayBuffer;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return (-1);
      case KEY_DERIVATION_ALGORITHM:
        return new AlgorithmIdentifier();
      case KEY_ENCRYPTION_ALGORITHM:
        return new AlgorithmIdentifier();
      case ENCRYPTED_KEY:
        return new asn1js.OctetString();
      case PASSWORD:
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
      case VERSION:
        return (memberValue === (-1));
      case KEY_DERIVATION_ALGORITHM:
      case KEY_ENCRYPTION_ALGORITHM:
        return ((memberValue.algorithmId === EMPTY_STRING) && (("algorithmParams" in memberValue) === false));
      case ENCRYPTED_KEY:
        return (memberValue.isEqual(PasswordRecipientinfo.defaultValues(ENCRYPTED_KEY)));
      case PASSWORD:
        return (memberValue.byteLength === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PasswordRecipientInfo ::= SEQUENCE {
   *    version CMSVersion,   -- Always set to 0
   *    keyDerivationAlgorithm [0] KeyDerivationAlgorithmIdentifier OPTIONAL,
   *    keyEncryptionAlgorithm KeyEncryptionAlgorithmIdentifier,
   *    encryptedKey EncryptedKey }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    keyDerivationAlgorithm?: string;
    keyEncryptionAlgorithm?: AlgorithmIdentifierSchema;
    encryptedKey?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        new asn1js.Constructed({
          name: (names.keyDerivationAlgorithm || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: AlgorithmIdentifier.schema().valueBlock.value
        }),
        AlgorithmIdentifier.schema(names.keyEncryptionAlgorithm || {}),
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
      PasswordRecipientinfo.schema({
        names: {
          version: VERSION,
          keyDerivationAlgorithm: KEY_DERIVATION_ALGORITHM,
          keyEncryptionAlgorithm: {
            names: {
              blockName: KEY_ENCRYPTION_ALGORITHM
            }
          },
          encryptedKey: ENCRYPTED_KEY
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;

    if (KEY_DERIVATION_ALGORITHM in asn1.result) {
      this.keyDerivationAlgorithm = new AlgorithmIdentifier({
        schema: new asn1js.Sequence({
          value: asn1.result.keyDerivationAlgorithm.valueBlock.value
        })
      });
    }
    this.keyEncryptionAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.keyEncryptionAlgorithm });
    this.encryptedKey = asn1.result.encryptedKey;
  }

  public toSchema(): asn1js.Sequence {
    //#region Create output array for sequence
    const outputArray = [];

    outputArray.push(new asn1js.Integer({ value: this.version }));

    if (this.keyDerivationAlgorithm) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: this.keyDerivationAlgorithm.toSchema().valueBlock.value
      }));
    }

    outputArray.push(this.keyEncryptionAlgorithm.toSchema());
    outputArray.push(this.encryptedKey);
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): PasswordRecipientInfoJson {
    const res: PasswordRecipientInfoJson = {
      version: this.version,
      keyEncryptionAlgorithm: this.keyEncryptionAlgorithm.toJSON(),
      encryptedKey: this.encryptedKey.toJSON(),
    };

    if (this.keyDerivationAlgorithm) {
      res.keyDerivationAlgorithm = this.keyDerivationAlgorithm.toJSON();
    }

    return res;
  }

}
