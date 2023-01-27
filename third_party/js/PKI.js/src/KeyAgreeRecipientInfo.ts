import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { OriginatorIdentifierOrKey, OriginatorIdentifierOrKeyJson, OriginatorIdentifierOrKeySchema } from "./OriginatorIdentifierOrKey";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { RecipientEncryptedKeys, RecipientEncryptedKeysJson, RecipientEncryptedKeysSchema } from "./RecipientEncryptedKeys";
import { Certificate } from "./Certificate";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const VERSION = "version";
const ORIGINATOR = "originator";
const UKM = "ukm";
const KEY_ENCRYPTION_ALGORITHM = "keyEncryptionAlgorithm";
const RECIPIENT_ENCRYPTED_KEY = "recipientEncryptedKeys";
const RECIPIENT_CERTIFICATE = "recipientCertificate";
const RECIPIENT_PUBLIC_KEY = "recipientPublicKey";
const CLEAR_PROPS = [
  VERSION,
  ORIGINATOR,
  UKM,
  KEY_ENCRYPTION_ALGORITHM,
  RECIPIENT_ENCRYPTED_KEY,
];

export interface IKeyAgreeRecipientInfo {
  version: number;
  originator: OriginatorIdentifierOrKey;
  ukm?: asn1js.OctetString;
  keyEncryptionAlgorithm: AlgorithmIdentifier;
  recipientEncryptedKeys: RecipientEncryptedKeys;
  recipientCertificate: Certificate;
  recipientPublicKey: CryptoKey | null;
}

export interface KeyAgreeRecipientInfoJson {
  version: number;
  originator: OriginatorIdentifierOrKeyJson;
  ukm?: asn1js.OctetStringJson;
  keyEncryptionAlgorithm: AlgorithmIdentifierJson;
  recipientEncryptedKeys: RecipientEncryptedKeysJson;
}

export type KeyAgreeRecipientInfoParameters = PkiObjectParameters & Partial<IKeyAgreeRecipientInfo>;

/**
 * Represents the KeyAgreeRecipientInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class KeyAgreeRecipientInfo extends PkiObject implements IKeyAgreeRecipientInfo {

  public static override CLASS_NAME = "KeyAgreeRecipientInfo";

  public version!: number;
  public originator!: OriginatorIdentifierOrKey;
  public ukm?: asn1js.OctetString;
  public keyEncryptionAlgorithm!: AlgorithmIdentifier;
  public recipientEncryptedKeys!: RecipientEncryptedKeys;
  public recipientCertificate!: Certificate;
  public recipientPublicKey!: CryptoKey | null;

  /**
   * Initializes a new instance of the {@link KeyAgreeRecipientInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: KeyAgreeRecipientInfoParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, KeyAgreeRecipientInfo.defaultValues(VERSION));
    this.originator = pvutils.getParametersValue(parameters, ORIGINATOR, KeyAgreeRecipientInfo.defaultValues(ORIGINATOR));
    if (UKM in parameters) {
      this.ukm = pvutils.getParametersValue(parameters, UKM, KeyAgreeRecipientInfo.defaultValues(UKM));
    }
    this.keyEncryptionAlgorithm = pvutils.getParametersValue(parameters, KEY_ENCRYPTION_ALGORITHM, KeyAgreeRecipientInfo.defaultValues(KEY_ENCRYPTION_ALGORITHM));
    this.recipientEncryptedKeys = pvutils.getParametersValue(parameters, RECIPIENT_ENCRYPTED_KEY, KeyAgreeRecipientInfo.defaultValues(RECIPIENT_ENCRYPTED_KEY));
    this.recipientCertificate = pvutils.getParametersValue(parameters, RECIPIENT_CERTIFICATE, KeyAgreeRecipientInfo.defaultValues(RECIPIENT_CERTIFICATE));
    this.recipientPublicKey = pvutils.getParametersValue(parameters, RECIPIENT_PUBLIC_KEY, KeyAgreeRecipientInfo.defaultValues(RECIPIENT_PUBLIC_KEY));

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
  public static override defaultValues(memberName: typeof ORIGINATOR): OriginatorIdentifierOrKey;
  public static override defaultValues(memberName: typeof UKM): asn1js.OctetString;
  public static override defaultValues(memberName: typeof KEY_ENCRYPTION_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof RECIPIENT_ENCRYPTED_KEY): RecipientEncryptedKeys;
  public static override defaultValues(memberName: typeof RECIPIENT_CERTIFICATE): Certificate;
  public static override defaultValues(memberName: typeof RECIPIENT_PUBLIC_KEY): null;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case ORIGINATOR:
        return new OriginatorIdentifierOrKey();
      case UKM:
        return new asn1js.OctetString();
      case KEY_ENCRYPTION_ALGORITHM:
        return new AlgorithmIdentifier();
      case RECIPIENT_ENCRYPTED_KEY:
        return new RecipientEncryptedKeys();
      case RECIPIENT_CERTIFICATE:
        return new Certificate();
      case RECIPIENT_PUBLIC_KEY:
        return null;
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
        return (memberValue === 0);
      case ORIGINATOR:
        return ((memberValue.variant === (-1)) && (("value" in memberValue) === false));
      case UKM:
        return (memberValue.isEqual(KeyAgreeRecipientInfo.defaultValues(UKM)));
      case KEY_ENCRYPTION_ALGORITHM:
        return ((memberValue.algorithmId === EMPTY_STRING) && (("algorithmParams" in memberValue) === false));
      case RECIPIENT_ENCRYPTED_KEY:
        return (memberValue.encryptedKeys.length === 0);
      case RECIPIENT_CERTIFICATE:
        return false; // For now leave it as is
      case RECIPIENT_PUBLIC_KEY:
        return false;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * KeyAgreeRecipientInfo ::= SEQUENCE {
   *    version CMSVersion,  -- always set to 3
   *    originator [0] EXPLICIT OriginatorIdentifierOrKey,
   *    ukm [1] EXPLICIT UserKeyingMaterial OPTIONAL,
   *    keyEncryptionAlgorithm KeyEncryptionAlgorithmIdentifier,
   *    recipientEncryptedKeys RecipientEncryptedKeys }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    originator?: OriginatorIdentifierOrKeySchema;
    ukm?: string;
    keyEncryptionAlgorithm?: AlgorithmIdentifierSchema;
    recipientEncryptedKeys?: RecipientEncryptedKeysSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: names.blockName || EMPTY_STRING,
      value: [
        new asn1js.Integer({ name: names.version || EMPTY_STRING }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            OriginatorIdentifierOrKey.schema(names.originator || {})
          ]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [new asn1js.OctetString({ name: names.ukm || EMPTY_STRING })]
        }),
        AlgorithmIdentifier.schema(names.keyEncryptionAlgorithm || {}),
        RecipientEncryptedKeys.schema(names.recipientEncryptedKeys || {})
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      KeyAgreeRecipientInfo.schema({
        names: {
          version: VERSION,
          originator: {
            names: {
              blockName: ORIGINATOR
            }
          },
          ukm: UKM,
          keyEncryptionAlgorithm: {
            names: {
              blockName: KEY_ENCRYPTION_ALGORITHM
            }
          },
          recipientEncryptedKeys: {
            names: {
              blockName: RECIPIENT_ENCRYPTED_KEY
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;
    this.originator = new OriginatorIdentifierOrKey({ schema: asn1.result.originator });
    if (UKM in asn1.result)
      this.ukm = asn1.result.ukm;
    this.keyEncryptionAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.keyEncryptionAlgorithm });
    this.recipientEncryptedKeys = new RecipientEncryptedKeys({ schema: asn1.result.recipientEncryptedKeys });
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for final sequence
    const outputArray = [];

    outputArray.push(new asn1js.Integer({ value: this.version }));
    outputArray.push(new asn1js.Constructed({
      idBlock: {
        tagClass: 3, // CONTEXT-SPECIFIC
        tagNumber: 0 // [0]
      },
      value: [this.originator.toSchema()]
    }));

    if (this.ukm) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: [this.ukm]
      }));
    }

    outputArray.push(this.keyEncryptionAlgorithm.toSchema());
    outputArray.push(this.recipientEncryptedKeys.toSchema());
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  /**
   * Conversion for the class to JSON object
   * @returns
   */
  public toJSON(): KeyAgreeRecipientInfoJson {
    const res: KeyAgreeRecipientInfoJson = {
      version: this.version,
      originator: this.originator.toJSON(),
      keyEncryptionAlgorithm: this.keyEncryptionAlgorithm.toJSON(),
      recipientEncryptedKeys: this.recipientEncryptedKeys.toJSON(),
    };

    if (this.ukm) {
      res.ukm = this.ukm.toJSON();
    }

    return res;
  }

}
