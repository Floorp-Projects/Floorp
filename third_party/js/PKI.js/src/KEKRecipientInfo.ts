import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { KEKIdentifier, KEKIdentifierJson, KEKIdentifierSchema } from "./KEKIdentifier";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";

const VERSION = "version";
const KEK_ID = "kekid";
const KEY_ENCRYPTION_ALGORITHM = "keyEncryptionAlgorithm";
const ENCRYPTED_KEY = "encryptedKey";
const PER_DEFINED_KEK = "preDefinedKEK";
const CLEAR_PROPS = [
  VERSION,
  KEK_ID,
  KEY_ENCRYPTION_ALGORITHM,
  ENCRYPTED_KEY,
];

export interface IKEKRecipientInfo {
  version: number;
  kekid: KEKIdentifier;
  keyEncryptionAlgorithm: AlgorithmIdentifier;
  encryptedKey: asn1js.OctetString;
  preDefinedKEK: ArrayBuffer;
}

export interface KEKRecipientInfoJson {
  version: number;
  kekid: KEKIdentifierJson;
  keyEncryptionAlgorithm: AlgorithmIdentifierJson;
  encryptedKey: asn1js.OctetStringJson;
}

export type KEKRecipientInfoParameters = PkiObjectParameters & Partial<IKEKRecipientInfo>;

/**
 * Represents the KEKRecipientInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class KEKRecipientInfo extends PkiObject implements IKEKRecipientInfo {

  public static override CLASS_NAME = "KEKRecipientInfo";

  public version!: number;
  public kekid!: KEKIdentifier;
  public keyEncryptionAlgorithm!: AlgorithmIdentifier;
  public encryptedKey!: asn1js.OctetString;
  public preDefinedKEK!: ArrayBuffer;

  /**
   * Initializes a new instance of the {@link KEKRecipientInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: KEKRecipientInfoParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, KEKRecipientInfo.defaultValues(VERSION));
    this.kekid = pvutils.getParametersValue(parameters, KEK_ID, KEKRecipientInfo.defaultValues(KEK_ID));
    this.keyEncryptionAlgorithm = pvutils.getParametersValue(parameters, KEY_ENCRYPTION_ALGORITHM, KEKRecipientInfo.defaultValues(KEY_ENCRYPTION_ALGORITHM));
    this.encryptedKey = pvutils.getParametersValue(parameters, ENCRYPTED_KEY, KEKRecipientInfo.defaultValues(ENCRYPTED_KEY));
    this.preDefinedKEK = pvutils.getParametersValue(parameters, PER_DEFINED_KEK, KEKRecipientInfo.defaultValues(PER_DEFINED_KEK));

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
  public static override defaultValues(memberName: typeof KEK_ID): KEKIdentifier;
  public static override defaultValues(memberName: typeof KEY_ENCRYPTION_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ENCRYPTED_KEY): asn1js.OctetString;
  public static override defaultValues(memberName: typeof PER_DEFINED_KEK): ArrayBuffer;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case KEK_ID:
        return new KEKIdentifier();
      case KEY_ENCRYPTION_ALGORITHM:
        return new AlgorithmIdentifier();
      case ENCRYPTED_KEY:
        return new asn1js.OctetString();
      case PER_DEFINED_KEK:
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
      case "KEKRecipientInfo":
        return (memberValue === KEKRecipientInfo.defaultValues(VERSION));
      case KEK_ID:
        return ((memberValue.compareWithDefault("keyIdentifier", memberValue.keyIdentifier)) &&
          (("date" in memberValue) === false) &&
          (("other" in memberValue) === false));
      case KEY_ENCRYPTION_ALGORITHM:
        return ((memberValue.algorithmId === EMPTY_STRING) && (("algorithmParams" in memberValue) === false));
      case ENCRYPTED_KEY:
        return (memberValue.isEqual(KEKRecipientInfo.defaultValues(ENCRYPTED_KEY)));
      case PER_DEFINED_KEK:
        return (memberValue.byteLength === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * KEKRecipientInfo ::= SEQUENCE {
   *    version CMSVersion,  -- always set to 4
   *    kekid KEKIdentifier,
   *    keyEncryptionAlgorithm KeyEncryptionAlgorithmIdentifier,
   *    encryptedKey EncryptedKey }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    kekid?: KEKIdentifierSchema;
    keyEncryptionAlgorithm?: AlgorithmIdentifierSchema;
    encryptedKey?: string;
  }> = {}): Schema.SchemaType {
    /**
     * @type {Object}
     * @property {string} [blockName]
     * @property {string} [version]
     * @property {string} [kekid]
     * @property {string} [keyEncryptionAlgorithm]
     * @property {string} [encryptedKey]
     */
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        KEKIdentifier.schema(names.kekid || {}),
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
      KEKRecipientInfo.schema({
        names: {
          version: VERSION,
          kekid: {
            names: {
              blockName: KEK_ID
            }
          },
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
    this.kekid = new KEKIdentifier({ schema: asn1.result.kekid });
    this.keyEncryptionAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.keyEncryptionAlgorithm });
    this.encryptedKey = asn1.result.encryptedKey;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.Integer({ value: this.version }),
        this.kekid.toSchema(),
        this.keyEncryptionAlgorithm.toSchema(),
        this.encryptedKey
      ]
    }));
  }

  public toJSON(): KEKRecipientInfoJson {
    return {
      version: this.version,
      kekid: this.kekid.toJSON(),
      keyEncryptionAlgorithm: this.keyEncryptionAlgorithm.toJSON(),
      encryptedKey: this.encryptedKey.toJSON(),
    };
  }

}
