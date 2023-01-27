import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as common from "./common";
import { ContentInfo, ContentInfoJson, ContentInfoSchema } from "./ContentInfo";
import { MacData, MacDataJson, MacDataSchema } from "./MacData";
import { DigestInfo } from "./DigestInfo";
import { AlgorithmIdentifier } from "./AlgorithmIdentifier";
import { SignedData } from "./SignedData";
import { EncapsulatedContentInfo } from "./EncapsulatedContentInfo";
import { Attribute } from "./Attribute";
import { SignerInfo } from "./SignerInfo";
import { IssuerAndSerialNumber } from "./IssuerAndSerialNumber";
import { SignedAndUnsignedAttributes } from "./SignedAndUnsignedAttributes";
import { AuthenticatedSafe } from "./AuthenticatedSafe";
import * as Schema from "./Schema";
import { Certificate } from "./Certificate";
import { ArgumentError, AsnError, ParameterError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { BufferSourceConverter } from "pvtsutils";
import { EMPTY_STRING } from "./constants";

const VERSION = "version";
const AUTH_SAFE = "authSafe";
const MAC_DATA = "macData";
const PARSED_VALUE = "parsedValue";
const CLERA_PROPS = [
  VERSION,
  AUTH_SAFE,
  MAC_DATA
];

export interface IPFX {
  version: number;
  authSafe: ContentInfo;
  macData?: MacData;
  parsedValue?: PFXParsedValue;
}

export interface PFXJson {
  version: number;
  authSafe: ContentInfoJson;
  macData?: MacDataJson;
}

export type PFXParameters = PkiObjectParameters & Partial<IPFX>;

export interface PFXParsedValue {
  authenticatedSafe?: AuthenticatedSafe;
  integrityMode?: number;
}

export type MakeInternalValuesParams =
  {
    // empty
  }
  |
  {
    iterations: number;
    pbkdf2HashAlgorithm: Algorithm;
    hmacHashAlgorithm: string;
    password: ArrayBuffer;
  }
  |
  {
    signingCertificate: Certificate;
    privateKey: CryptoKey;
    hashAlgorithm: string;
  };

/**
 * Represents the PFX structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class PFX extends PkiObject implements IPFX {

  public static override CLASS_NAME = "PFX";

  public version!: number;
  public authSafe!: ContentInfo;
  public macData?: MacData;
  public parsedValue?: PFXParsedValue;

  /**
   * Initializes a new instance of the {@link PFX} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PFXParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, PFX.defaultValues(VERSION));
    this.authSafe = pvutils.getParametersValue(parameters, AUTH_SAFE, PFX.defaultValues(AUTH_SAFE));
    if (MAC_DATA in parameters) {
      this.macData = pvutils.getParametersValue(parameters, MAC_DATA, PFX.defaultValues(MAC_DATA));
    }
    if (PARSED_VALUE in parameters) {
      this.parsedValue = pvutils.getParametersValue(parameters, PARSED_VALUE, PFX.defaultValues(PARSED_VALUE));
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
  public static override defaultValues(memberName: typeof AUTH_SAFE): ContentInfo;
  public static override defaultValues(memberName: typeof MAC_DATA): MacData;
  public static override defaultValues(memberName: typeof PARSED_VALUE): PFXParsedValue;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 3;
      case AUTH_SAFE:
        return (new ContentInfo());
      case MAC_DATA:
        return (new MacData());
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
      case VERSION:
        return (memberValue === PFX.defaultValues(memberName));
      case AUTH_SAFE:
        return ((ContentInfo.compareWithDefault("contentType", memberValue.contentType)) &&
          (ContentInfo.compareWithDefault("content", memberValue.content)));
      case MAC_DATA:
        return ((MacData.compareWithDefault("mac", memberValue.mac)) &&
          (MacData.compareWithDefault("macSalt", memberValue.macSalt)) &&
          (MacData.compareWithDefault("iterations", memberValue.iterations)));
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
   * PFX ::= SEQUENCE {
   *    version     INTEGER {v3(3)}(v3,...),
   *    authSafe    ContentInfo,
   *    macData     MacData OPTIONAL
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    authSafe?: ContentInfoSchema;
    macData?: MacDataSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || VERSION) }),
        ContentInfo.schema(names.authSafe || {
          names: {
            blockName: AUTH_SAFE
          }
        }),
        MacData.schema(names.macData || {
          names: {
            blockName: MAC_DATA,
            optional: true
          }
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLERA_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      PFX.schema({
        names: {
          version: VERSION,
          authSafe: {
            names: {
              blockName: AUTH_SAFE
            }
          },
          macData: {
            names: {
              blockName: MAC_DATA
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;
    this.authSafe = new ContentInfo({ schema: asn1.result.authSafe });
    if (MAC_DATA in asn1.result)
      this.macData = new MacData({ schema: asn1.result.macData });
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    const outputArray = [
      new asn1js.Integer({ value: this.version }),
      this.authSafe.toSchema()
    ];

    if (this.macData) {
      outputArray.push(this.macData.toSchema());
    }

    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): PFXJson {
    const output: PFXJson = {
      version: this.version,
      authSafe: this.authSafe.toJSON()
    };

    if (this.macData) {
      output.macData = this.macData.toJSON();
    }

    return output;
  }

  /**
   * Making ContentInfo from PARSED_VALUE object
   * @param parameters Parameters, specific to each "integrity mode"
   * @param crypto Crypto engine
   */
  public async makeInternalValues(parameters: MakeInternalValuesParams = {}, crypto = common.getCrypto(true)) {
    //#region Check mandatory parameter
    ArgumentError.assert(parameters, "parameters", "object");
    if (!this.parsedValue) {
      throw new Error("Please call \"parseValues\" function first in order to make \"parsedValue\" data");
    }
    ParameterError.assertEmpty(this.parsedValue.integrityMode, "integrityMode", "parsedValue");
    ParameterError.assertEmpty(this.parsedValue.authenticatedSafe, "authenticatedSafe", "parsedValue");
    //#endregion

    //#region Makes values for each particular integrity mode
    switch (this.parsedValue.integrityMode) {
      //#region HMAC-based integrity
      case 0:
        {
          //#region Check additional mandatory parameters
          if (!("iterations" in parameters))
            throw new ParameterError("iterations");
          ParameterError.assertEmpty(parameters.pbkdf2HashAlgorithm, "pbkdf2HashAlgorithm");
          ParameterError.assertEmpty(parameters.hmacHashAlgorithm, "hmacHashAlgorithm");
          ParameterError.assertEmpty(parameters.password, "password");
          //#endregion

          //#region Initial variables
          const saltBuffer = new ArrayBuffer(64);
          const saltView = new Uint8Array(saltBuffer);

          crypto.getRandomValues(saltView);

          const data = this.parsedValue.authenticatedSafe.toSchema().toBER(false);

          this.authSafe = new ContentInfo({
            contentType: ContentInfo.DATA,
            content: new asn1js.OctetString({ valueHex: data })
          });
          //#endregion

          //#region Call current crypto engine for making HMAC-based data stamp
          const result = await crypto.stampDataWithPassword({
            password: parameters.password,
            hashAlgorithm: parameters.hmacHashAlgorithm,
            salt: saltBuffer,
            iterationCount: parameters.iterations,
            contentToStamp: data
          });
          //#endregion

          //#region Make MAC_DATA values
          this.macData = new MacData({
            mac: new DigestInfo({
              digestAlgorithm: new AlgorithmIdentifier({
                algorithmId: crypto.getOIDByAlgorithm({ name: parameters.hmacHashAlgorithm }, true, "hmacHashAlgorithm"),
              }),
              digest: new asn1js.OctetString({ valueHex: result })
            }),
            macSalt: new asn1js.OctetString({ valueHex: saltBuffer }),
            iterations: parameters.iterations
          });
          //#endregion
          //#endregion
        }
        break;
      //#endregion
      //#region publicKey-based integrity
      case 1:
        {
          //#region Check additional mandatory parameters
          if (!("signingCertificate" in parameters)) {
            throw new ParameterError("signingCertificate");
          }
          ParameterError.assertEmpty(parameters.privateKey, "privateKey");
          ParameterError.assertEmpty(parameters.hashAlgorithm, "hashAlgorithm");
          //#endregion

          //#region Making data to be signed
          // NOTE: all internal data for "authenticatedSafe" must be already prepared.
          // Thus user must call "makeValues" for all internal "SafeContent" value with appropriate parameters.
          // Or user can choose to use values from initial parsing of existing PKCS#12 data.

          const toBeSigned = this.parsedValue.authenticatedSafe.toSchema().toBER(false);
          //#endregion

          //#region Initial variables
          const cmsSigned = new SignedData({
            version: 1,
            encapContentInfo: new EncapsulatedContentInfo({
              eContentType: "1.2.840.113549.1.7.1", // "data" content type
              eContent: new asn1js.OctetString({ valueHex: toBeSigned })
            }),
            certificates: [parameters.signingCertificate]
          });
          //#endregion

          //#region Making additional attributes for CMS Signed Data
          //#region Create a message digest
          const result = await crypto.digest({ name: parameters.hashAlgorithm }, new Uint8Array(toBeSigned));
          //#endregion

          //#region Combine all signed extensions
          //#region Initial variables
          const signedAttr: Attribute[] = [];
          //#endregion

          //#region contentType
          signedAttr.push(new Attribute({
            type: "1.2.840.113549.1.9.3",
            values: [
              new asn1js.ObjectIdentifier({ value: "1.2.840.113549.1.7.1" })
            ]
          }));
          //#endregion
          //#region signingTime
          signedAttr.push(new Attribute({
            type: "1.2.840.113549.1.9.5",
            values: [
              new asn1js.UTCTime({ valueDate: new Date() })
            ]
          }));
          //#endregion
          //#region messageDigest
          signedAttr.push(new Attribute({
            type: "1.2.840.113549.1.9.4",
            values: [
              new asn1js.OctetString({ valueHex: result })
            ]
          }));
          //#endregion

          //#region Making final value for "SignerInfo" type
          cmsSigned.signerInfos.push(new SignerInfo({
            version: 1,
            sid: new IssuerAndSerialNumber({
              issuer: parameters.signingCertificate.issuer,
              serialNumber: parameters.signingCertificate.serialNumber
            }),
            signedAttrs: new SignedAndUnsignedAttributes({
              type: 0,
              attributes: signedAttr
            })
          }));
          //#endregion
          //#endregion
          //#endregion

          //#region Signing CMS Signed Data
          await cmsSigned.sign(parameters.privateKey, 0, parameters.hashAlgorithm, undefined, crypto);
          //#endregion

          //#region Making final CMS_CONTENT_INFO type
          this.authSafe = new ContentInfo({
            contentType: "1.2.840.113549.1.7.2",
            content: cmsSigned.toSchema(true)
          });
          //#endregion
        }
        break;
      //#endregion
      //#region default
      default:
        throw new Error(`Parameter "integrityMode" has unknown value: ${this.parsedValue.integrityMode}`);
      //#endregion
    }
    //#endregion
  }

  public async parseInternalValues(parameters: {
    checkIntegrity?: boolean;
    password?: ArrayBuffer;
  }, crypto = common.getCrypto(true)) {
    //#region Check input data from "parameters"
    ArgumentError.assert(parameters, "parameters", "object");

    if (parameters.checkIntegrity === undefined) {
      parameters.checkIntegrity = true;
    }
    //#endregion

    //#region Create value for "this.parsedValue.authenticatedSafe" and check integrity
    this.parsedValue = {};

    switch (this.authSafe.contentType) {
      //#region data
      case ContentInfo.DATA:
        {
          //#region Check additional mandatory parameters
          ParameterError.assertEmpty(parameters.password, "password");
          //#endregion

          //#region Integrity based on HMAC
          this.parsedValue.integrityMode = 0;
          //#endregion

          //#region Check that we do have OCTETSTRING as "content"
          ArgumentError.assert(this.authSafe.content, "authSafe.content", asn1js.OctetString);
          //#endregion

          //#region Check we have "constructive encoding" for AuthSafe content
          const authSafeContent = this.authSafe.content.getValue();
          //#endregion

          //#region Set "authenticatedSafe" value
          this.parsedValue.authenticatedSafe = AuthenticatedSafe.fromBER(authSafeContent);
          //#endregion

          //#region Check integrity
          if (parameters.checkIntegrity) {
            //#region Check that MAC_DATA exists
            if (!this.macData) {
              throw new Error("Absent \"macData\" value, can not check PKCS#12 data integrity");
            }
            //#endregion

            //#region Initial variables
            const hashAlgorithm = crypto.getAlgorithmByOID(this.macData.mac.digestAlgorithm.algorithmId, true, "digestAlgorithm");
            //#endregion

            //#region Call current crypto engine for verifying HMAC-based data stamp
            const result = await crypto.verifyDataStampedWithPassword({
              password: parameters.password,
              hashAlgorithm: hashAlgorithm.name,
              salt: BufferSourceConverter.toArrayBuffer(this.macData.macSalt.valueBlock.valueHexView),
              iterationCount: this.macData.iterations || 1,
              contentToVerify: authSafeContent,
              signatureToVerify: BufferSourceConverter.toArrayBuffer(this.macData.mac.digest.valueBlock.valueHexView),
            });
            //#endregion

            //#region Verify HMAC signature
            if (!result) {
              throw new Error("Integrity for the PKCS#12 data is broken!");
            }
            //#endregion
          }
          //#endregion
        }
        break;
      //#endregion
      //#region signedData
      case ContentInfo.SIGNED_DATA:
        {
          //#region Integrity based on signature using public key
          this.parsedValue.integrityMode = 1;
          //#endregion

          //#region Parse CMS Signed Data
          const cmsSigned = new SignedData({ schema: this.authSafe.content });
          //#endregion

          //#region Check that we do have OCTET STRING as "content"
          const eContent = cmsSigned.encapContentInfo.eContent;
          ParameterError.assert(eContent, "eContent", "cmsSigned.encapContentInfo");
          ArgumentError.assert(eContent, "eContent", asn1js.OctetString);
          //#endregion

          //#region Create correct data block for verification
          const data = eContent.getValue();
          //#endregion

          //#region Set "authenticatedSafe" value
          this.parsedValue.authenticatedSafe = AuthenticatedSafe.fromBER(data);
          //#endregion

          //#region Check integrity
          const ok = await cmsSigned.verify({ signer: 0, checkChain: false }, crypto);
          if (!ok) {
            throw new Error("Integrity for the PKCS#12 data is broken!");
          }
          //#endregion
        }
        break;
      //#endregion
      //#region default
      default:
        throw new Error(`Incorrect value for "this.authSafe.contentType": ${this.authSafe.contentType}`);
      //#endregion
    }
    //#endregion
  }

}
