import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as common from "./common";
import { EncryptedContentInfo, EncryptedContentInfoJson, EncryptedContentInfoSchema } from "./EncryptedContentInfo";
import { Attribute, AttributeJson } from "./Attribute";
import * as Schema from "./Schema";
import { ArgumentError, AsnError } from "./errors";
import { CryptoEngineEncryptParams } from "./CryptoEngine/CryptoEngineInterface";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_STRING } from "./constants";

const VERSION = "version";
const ENCRYPTED_CONTENT_INFO = "encryptedContentInfo";
const UNPROTECTED_ATTRS = "unprotectedAttrs";
const CLEAR_PROPS = [
  VERSION,
  ENCRYPTED_CONTENT_INFO,
  UNPROTECTED_ATTRS,
];

export interface IEncryptedData {
  /**
   * Version number.
   *
   * If `unprotectedAttrs` is present, then the version MUST be 2. If `unprotectedAttrs` is absent, then version MUST be 0.
   */
  version: number;
  /**
   * Encrypted content information
   */
  encryptedContentInfo: EncryptedContentInfo;
  /**
   * Collection of attributes that are not encrypted
   */
  unprotectedAttrs?: Attribute[];
}

export interface EncryptedDataJson {
  version: number;
  encryptedContentInfo: EncryptedContentInfoJson;
  unprotectedAttrs?: AttributeJson[];
}

export type EncryptedDataParameters = PkiObjectParameters & Partial<IEncryptedData>;

export type EncryptedDataEncryptParams = Omit<CryptoEngineEncryptParams, "contentType">;

/**
 * Represents the EncryptedData structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 *
 * @example The following example demonstrates how to create and encrypt CMS Encrypted Data
 * ```js
 * const cmsEncrypted = new pkijs.EncryptedData();
 *
 * await cmsEncrypted.encrypt({
 *   contentEncryptionAlgorithm: {
 *     name: "AES-GCM",
 *     length: 256,
 *   },
 *   hmacHashAlgorithm: "SHA-256",
 *   iterationCount: 1000,
 *   password: password,
 *   contentToEncrypt: dataToEncrypt,
 * });
 *
 * // Add Encrypted Data into CMS Content Info
 * const cmsContent = new pkijs.ContentInfo();
 * cmsContent.contentType = pkijs.ContentInfo.ENCRYPTED_DATA;
 * cmsContent.content = cmsEncrypted.toSchema();
 *
 * const cmsContentRaw = cmsContent.toSchema().toBER();
 * ```
 *
 * @example The following example demonstrates how to decrypt CMS Encrypted Data
 * ```js
 * // Parse CMS Content Info
 * const cmsContent = pkijs.ContentInfo.fromBER(cmsContentRaw);
 * if (cmsContent.contentType !== pkijs.ContentInfo.ENCRYPTED_DATA) {
 *   throw new Error("CMS is not Encrypted Data");
 * }
 * // Parse CMS Encrypted Data
 * const cmsEncrypted = new pkijs.EncryptedData({ schema: cmsContent.content });
 *
 * // Decrypt data
 * const decryptedData = await cmsEncrypted.decrypt({
 *   password: password,
 * });
 * ```
 */
export class EncryptedData extends PkiObject implements IEncryptedData {

  public static override CLASS_NAME = "EncryptedData";

  public version!: number;
  public encryptedContentInfo!: EncryptedContentInfo;
  public unprotectedAttrs?: Attribute[];

  /**
   * Initializes a new instance of the {@link EncryptedData} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: EncryptedDataParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, EncryptedData.defaultValues(VERSION));
    this.encryptedContentInfo = pvutils.getParametersValue(parameters, ENCRYPTED_CONTENT_INFO, EncryptedData.defaultValues(ENCRYPTED_CONTENT_INFO));
    if (UNPROTECTED_ATTRS in parameters) {
      this.unprotectedAttrs = pvutils.getParametersValue(parameters, UNPROTECTED_ATTRS, EncryptedData.defaultValues(UNPROTECTED_ATTRS));
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
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof ENCRYPTED_CONTENT_INFO): EncryptedContentInfo;
  public static override defaultValues(memberName: typeof UNPROTECTED_ATTRS): Attribute[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case ENCRYPTED_CONTENT_INFO:
        return new EncryptedContentInfo();
      case UNPROTECTED_ATTRS:
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
      case VERSION:
        return (memberValue === 0);
      case ENCRYPTED_CONTENT_INFO:
        // TODO move to isEmpty method
        return ((EncryptedContentInfo.compareWithDefault("contentType", memberValue.contentType)) &&
          (EncryptedContentInfo.compareWithDefault("contentEncryptionAlgorithm", memberValue.contentEncryptionAlgorithm)) &&
          (EncryptedContentInfo.compareWithDefault("encryptedContent", memberValue.encryptedContent)));
      case UNPROTECTED_ATTRS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * EncryptedData ::= SEQUENCE {
   *    version CMSVersion,
   *    encryptedContentInfo EncryptedContentInfo,
   *    unprotectedAttrs [1] IMPLICIT UnprotectedAttributes OPTIONAL }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    encryptedContentInfo?: EncryptedContentInfoSchema;
    unprotectedAttrs?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        EncryptedContentInfo.schema(names.encryptedContentInfo || {}),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [
            new asn1js.Repeated({
              name: (names.unprotectedAttrs || EMPTY_STRING),
              value: Attribute.schema()
            })
          ]
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
      EncryptedData.schema({
        names: {
          version: VERSION,
          encryptedContentInfo: {
            names: {
              blockName: ENCRYPTED_CONTENT_INFO
            }
          },
          unprotectedAttrs: UNPROTECTED_ATTRS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;
    this.encryptedContentInfo = new EncryptedContentInfo({ schema: asn1.result.encryptedContentInfo });
    if (UNPROTECTED_ATTRS in asn1.result)
      this.unprotectedAttrs = Array.from(asn1.result.unprotectedAttrs, element => new Attribute({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(new asn1js.Integer({ value: this.version }));
    outputArray.push(this.encryptedContentInfo.toSchema());

    if (this.unprotectedAttrs) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: Array.from(this.unprotectedAttrs, o => o.toSchema())
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): EncryptedDataJson {
    const res: EncryptedDataJson = {
      version: this.version,
      encryptedContentInfo: this.encryptedContentInfo.toJSON()
    };

    if (this.unprotectedAttrs)
      res.unprotectedAttrs = Array.from(this.unprotectedAttrs, o => o.toJSON());

    return res;
  }

  /**
   * Creates a new CMS Encrypted Data content
   * @param parameters Parameters necessary for encryption
   */
  public async encrypt(parameters: EncryptedDataEncryptParams): Promise<void> {
    //#region Check for input parameters
    ArgumentError.assert(parameters, "parameters", "object");
    //#endregion

    //#region Set "contentType" parameter
    const encryptParams: CryptoEngineEncryptParams = {
      ...parameters,
      contentType: "1.2.840.113549.1.7.1",
    };
    //#endregion

    this.encryptedContentInfo = await common.getCrypto(true).encryptEncryptedContentInfo(encryptParams);
  }

  /**
   * Creates a new CMS Encrypted Data content
   * @param parameters Parameters necessary for encryption
   * @param crypto Crypto engine
   * @returns Returns decrypted raw data
   */
  async decrypt(parameters: {
    password: ArrayBuffer;
  }, crypto = common.getCrypto(true)): Promise<ArrayBuffer> {
    // Check for input parameters
    ArgumentError.assert(parameters, "parameters", "object");

    // Set ENCRYPTED_CONTENT_INFO value
    const decryptParams = {
      ...parameters,
      encryptedContentInfo: this.encryptedContentInfo,
    };

    return crypto.decryptEncryptedContentInfo(decryptParams);
  }

}
