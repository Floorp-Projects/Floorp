import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { Certificate } from "./Certificate";
import { RecipientIdentifier, RecipientIdentifierSchema } from "./RecipientIdentifier";
import { IssuerAndSerialNumber, IssuerAndSerialNumberJson } from "./IssuerAndSerialNumber";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const VERSION = "version";
const RID = "rid";
const KEY_ENCRYPTION_ALGORITHM = "keyEncryptionAlgorithm";
const ENCRYPTED_KEY = "encryptedKey";
const RECIPIENT_CERTIFICATE = "recipientCertificate";
const CLEAR_PROPS = [
  VERSION,
  RID,
  KEY_ENCRYPTION_ALGORITHM,
  ENCRYPTED_KEY,
];

export interface IKeyTransRecipientInfo {
  version: number;
  rid: RecipientIdentifierType;
  keyEncryptionAlgorithm: AlgorithmIdentifier;
  encryptedKey: asn1js.OctetString;
  recipientCertificate: Certificate;
}

export interface KeyTransRecipientInfoJson {
  version: number;
  rid: RecipientIdentifierMixedJson;
  keyEncryptionAlgorithm: AlgorithmIdentifierJson;
  encryptedKey: asn1js.OctetStringJson;
}

export type RecipientIdentifierType = IssuerAndSerialNumber | asn1js.OctetString;
export type RecipientIdentifierMixedJson = IssuerAndSerialNumberJson | asn1js.OctetStringJson;

export type KeyTransRecipientInfoParameters = PkiObjectParameters & Partial<IKeyTransRecipientInfo>;

/**
 * Represents the KeyTransRecipientInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class KeyTransRecipientInfo extends PkiObject implements IKeyTransRecipientInfo {

  public static override CLASS_NAME = "KeyTransRecipientInfo";

  public version!: number;
  public rid!: RecipientIdentifierType;
  public keyEncryptionAlgorithm!: AlgorithmIdentifier;
  public encryptedKey!: asn1js.OctetString;
  public recipientCertificate!: Certificate;

  /**
   * Initializes a new instance of the {@link KeyTransRecipientInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: KeyTransRecipientInfoParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, KeyTransRecipientInfo.defaultValues(VERSION));
    this.rid = pvutils.getParametersValue(parameters, RID, KeyTransRecipientInfo.defaultValues(RID));
    this.keyEncryptionAlgorithm = pvutils.getParametersValue(parameters, KEY_ENCRYPTION_ALGORITHM, KeyTransRecipientInfo.defaultValues(KEY_ENCRYPTION_ALGORITHM));
    this.encryptedKey = pvutils.getParametersValue(parameters, ENCRYPTED_KEY, KeyTransRecipientInfo.defaultValues(ENCRYPTED_KEY));
    this.recipientCertificate = pvutils.getParametersValue(parameters, RECIPIENT_CERTIFICATE, KeyTransRecipientInfo.defaultValues(RECIPIENT_CERTIFICATE));

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
  public static override defaultValues(memberName: typeof RID): RecipientIdentifierType;
  public static override defaultValues(memberName: typeof KEY_ENCRYPTION_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ENCRYPTED_KEY): asn1js.OctetString;
  public static override defaultValues(memberName: typeof RECIPIENT_CERTIFICATE): Certificate;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return (-1);
      case RID:
        return {};
      case KEY_ENCRYPTION_ALGORITHM:
        return new AlgorithmIdentifier();
      case ENCRYPTED_KEY:
        return new asn1js.OctetString();
      case RECIPIENT_CERTIFICATE:
        return new Certificate();
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
        return (memberValue === KeyTransRecipientInfo.defaultValues(VERSION));
      case RID:
        return (Object.keys(memberValue).length === 0);
      case KEY_ENCRYPTION_ALGORITHM:
      case ENCRYPTED_KEY:
        return memberValue.isEqual(KeyTransRecipientInfo.defaultValues(memberName as typeof ENCRYPTED_KEY));
      case RECIPIENT_CERTIFICATE:
        return false; // For now we do not need to compare any values with the RECIPIENT_CERTIFICATE
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * KeyTransRecipientInfo ::= SEQUENCE {
   *    version CMSVersion,  -- always set to 0 or 2
   *    rid RecipientIdentifier,
   *    keyEncryptionAlgorithm KeyEncryptionAlgorithmIdentifier,
   *    encryptedKey EncryptedKey }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    rid?: RecipientIdentifierSchema;
    keyEncryptionAlgorithm?: AlgorithmIdentifierSchema;
    encryptedKey?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        RecipientIdentifier.schema(names.rid || {}),
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
      KeyTransRecipientInfo.schema({
        names: {
          version: VERSION,
          rid: {
            names: {
              blockName: RID
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
    if (asn1.result.rid.idBlock.tagClass === 3) {
      this.rid = new asn1js.OctetString({ valueHex: asn1.result.rid.valueBlock.valueHex }); // SubjectKeyIdentifier
    } else {
      this.rid = new IssuerAndSerialNumber({ schema: asn1.result.rid });
    }
    this.keyEncryptionAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.keyEncryptionAlgorithm });
    this.encryptedKey = asn1.result.encryptedKey;
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (this.rid instanceof IssuerAndSerialNumber) {
      this.version = 0;

      outputArray.push(new asn1js.Integer({ value: this.version }));
      outputArray.push(this.rid.toSchema());
    }
    else {
      this.version = 2;

      outputArray.push(new asn1js.Integer({ value: this.version }));
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        valueHex: this.rid.valueBlock.valueHexView
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

  public toJSON(): KeyTransRecipientInfoJson {
    return {
      version: this.version,
      rid: this.rid.toJSON(),
      keyEncryptionAlgorithm: this.keyEncryptionAlgorithm.toJSON(),
      encryptedKey: this.encryptedKey.toJSON(),
    };
  }

}
