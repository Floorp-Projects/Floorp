import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EncryptedData, EncryptedDataEncryptParams } from "./EncryptedData";
import { EncryptedContentInfo } from "./EncryptedContentInfo";
import { PrivateKeyInfo } from "./PrivateKeyInfo";
import * as Schema from "./Schema";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_STRING } from "./constants";
import * as common from "./common";

const ENCRYPTION_ALGORITHM = "encryptionAlgorithm";
const ENCRYPTED_DATA = "encryptedData";
const PARSED_VALUE = "parsedValue";
const CLEAR_PROPS = [
  ENCRYPTION_ALGORITHM,
  ENCRYPTED_DATA,
];

export interface IPKCS8ShroudedKeyBag {
  encryptionAlgorithm: AlgorithmIdentifier;
  encryptedData: asn1js.OctetString;
  parsedValue?: PrivateKeyInfo;
}

export type PKCS8ShroudedKeyBagParameters = PkiObjectParameters & Partial<IPKCS8ShroudedKeyBag>;

export interface PKCS8ShroudedKeyBagJson {
  encryptionAlgorithm: AlgorithmIdentifierJson;
  encryptedData: asn1js.OctetStringJson;
}

type PKCS8ShroudedKeyBagMakeInternalValuesParams = Omit<EncryptedDataEncryptParams, "contentToEncrypt">;

/**
 * Represents the PKCS8ShroudedKeyBag structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class PKCS8ShroudedKeyBag extends PkiObject implements IPKCS8ShroudedKeyBag {

  public static override CLASS_NAME = "PKCS8ShroudedKeyBag";

  public encryptionAlgorithm!: AlgorithmIdentifier;
  public encryptedData!: asn1js.OctetString;
  public parsedValue?: PrivateKeyInfo;

  /**
   * Initializes a new instance of the {@link PKCS8ShroudedKeyBag} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PKCS8ShroudedKeyBagParameters = {}) {
    super();

    this.encryptionAlgorithm = pvutils.getParametersValue(parameters, ENCRYPTION_ALGORITHM, PKCS8ShroudedKeyBag.defaultValues(ENCRYPTION_ALGORITHM));
    this.encryptedData = pvutils.getParametersValue(parameters, ENCRYPTED_DATA, PKCS8ShroudedKeyBag.defaultValues(ENCRYPTED_DATA));
    if (PARSED_VALUE in parameters) {
      this.parsedValue = pvutils.getParametersValue(parameters, PARSED_VALUE, PKCS8ShroudedKeyBag.defaultValues(PARSED_VALUE));
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
  public static override defaultValues(memberName: typeof ENCRYPTION_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ENCRYPTED_DATA): asn1js.OctetString;
  public static override defaultValues(memberName: typeof PARSED_VALUE): PrivateKeyInfo;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ENCRYPTION_ALGORITHM:
        return (new AlgorithmIdentifier());
      case ENCRYPTED_DATA:
        return (new asn1js.OctetString());
      case PARSED_VALUE:
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
      case ENCRYPTION_ALGORITHM:
        return ((AlgorithmIdentifier.compareWithDefault("algorithmId", memberValue.algorithmId)) &&
          (("algorithmParams" in memberValue) === false));
      case ENCRYPTED_DATA:
        return (memberValue.isEqual(PKCS8ShroudedKeyBag.defaultValues(memberName)));
      case PARSED_VALUE:
        return ((memberValue instanceof Object) && (Object.keys(memberValue).length === 0));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PKCS8ShroudedKeyBag ::= EncryptedPrivateKeyInfo
   *
   * EncryptedPrivateKeyInfo ::= SEQUENCE {
   *    encryptionAlgorithm AlgorithmIdentifier {{KeyEncryptionAlgorithms}},
   *    encryptedData EncryptedData
   * }
   *
   * EncryptedData ::= OCTET STRING
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    encryptionAlgorithm?: AlgorithmIdentifierSchema;
    encryptedData?: string;
  }> = {}): Schema.SchemaType {
    /**
     * @type {Object}
     * @property {string} [blockName]
     * @property {string} [encryptionAlgorithm]
     * @property {string} [encryptedData]
     */
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.encryptionAlgorithm || {
          names: {
            blockName: ENCRYPTION_ALGORITHM
          }
        }),
        new asn1js.Choice({
          value: [
            new asn1js.OctetString({ name: (names.encryptedData || ENCRYPTED_DATA) }),
            new asn1js.OctetString({
              idBlock: {
                isConstructed: true
              },
              name: (names.encryptedData || ENCRYPTED_DATA)
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
      PKCS8ShroudedKeyBag.schema({
        names: {
          encryptionAlgorithm: {
            names: {
              blockName: ENCRYPTION_ALGORITHM
            }
          },
          encryptedData: ENCRYPTED_DATA
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.encryptionAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.encryptionAlgorithm });
    this.encryptedData = asn1.result.encryptedData;
  }

  public toSchema(): asn1js.Sequence {
    return (new asn1js.Sequence({
      value: [
        this.encryptionAlgorithm.toSchema(),
        this.encryptedData
      ]
    }));
  }

  public toJSON(): PKCS8ShroudedKeyBagJson {
    return {
      encryptionAlgorithm: this.encryptionAlgorithm.toJSON(),
      encryptedData: this.encryptedData.toJSON(),
    };
  }

  protected async parseInternalValues(parameters: {
    password: ArrayBuffer;
  }, crypto = common.getCrypto(true)) {
    //#region Initial variables
    const cmsEncrypted = new EncryptedData({
      encryptedContentInfo: new EncryptedContentInfo({
        contentEncryptionAlgorithm: this.encryptionAlgorithm,
        encryptedContent: this.encryptedData
      })
    });
    //#endregion

    //#region Decrypt internal data
    const decryptedData = await cmsEncrypted.decrypt(parameters, crypto);

    //#endregion

    //#region Initialize PARSED_VALUE with decrypted PKCS#8 private key

    this.parsedValue = PrivateKeyInfo.fromBER(decryptedData);
    //#endregion
  }

  public async makeInternalValues(parameters: PKCS8ShroudedKeyBagMakeInternalValuesParams): Promise<void> {
    //#region Check that we do have PARSED_VALUE
    if (!this.parsedValue) {
      throw new Error("Please initialize \"parsedValue\" first");
    }
    //#endregion

    //#region Initial variables
    const cmsEncrypted = new EncryptedData();
    //#endregion

    //#region Encrypt internal data
    const encryptParams: EncryptedDataEncryptParams = {
      ...parameters,
      contentToEncrypt: this.parsedValue.toSchema().toBER(false),
    };

    await cmsEncrypted.encrypt(encryptParams);
    if (!cmsEncrypted.encryptedContentInfo.encryptedContent) {
      throw new Error("The filed `encryptedContent` in EncryptedContentInfo is empty");
    }

    //#endregion

    //#region Initialize internal values
    this.encryptionAlgorithm = cmsEncrypted.encryptedContentInfo.contentEncryptionAlgorithm;
    this.encryptedData = cmsEncrypted.encryptedContentInfo.encryptedContent;
    //#endregion
  }

}
