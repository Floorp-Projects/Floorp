import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as common from "./common";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { ECPublicKey } from "./ECPublicKey";
import { RSAPublicKey } from "./RSAPublicKey";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const ALGORITHM = "algorithm";
const SUBJECT_PUBLIC_KEY = "subjectPublicKey";
const CLEAR_PROPS = [ALGORITHM, SUBJECT_PUBLIC_KEY];

export interface IPublicKeyInfo {
  /**
   * Algorithm identifier
   */
  algorithm: AlgorithmIdentifier;
  /**
   * Subject public key value
   */
  subjectPublicKey: asn1js.BitString;
  /**
   * Parsed public key value
   */
  parsedKey?: ECPublicKey | RSAPublicKey;
}
export type PublicKeyInfoParameters = PkiObjectParameters & Partial<IPublicKeyInfo> & { json?: JsonWebKey; };

export interface PublicKeyInfoJson {
  algorithm: AlgorithmIdentifierJson;
  subjectPublicKey: asn1js.BitStringJson;
}

export type PublicKeyInfoSchema = Schema.SchemaParameters<{
  algorithm?: AlgorithmIdentifierSchema;
  subjectPublicKey?: string;
}>;

/**
 * Represents the PublicKeyInfo structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class PublicKeyInfo extends PkiObject implements IPublicKeyInfo {

  public static override CLASS_NAME = "PublicKeyInfo";

  public algorithm!: AlgorithmIdentifier;
  public subjectPublicKey!: asn1js.BitString;
  private _parsedKey?: ECPublicKey | RSAPublicKey | null;
  public get parsedKey(): ECPublicKey | RSAPublicKey | undefined {
    if (this._parsedKey === undefined) {
      switch (this.algorithm.algorithmId) {
        // TODO Use fabric
        case "1.2.840.10045.2.1": // ECDSA
          if ("algorithmParams" in this.algorithm) {
            if (this.algorithm.algorithmParams.constructor.blockName() === asn1js.ObjectIdentifier.blockName()) {
              try {
                this._parsedKey = new ECPublicKey({
                  namedCurve: this.algorithm.algorithmParams.valueBlock.toString(),
                  schema: this.subjectPublicKey.valueBlock.valueHexView
                });
              }
              catch (ex) {
                // nothing
              } // Could be a problems during recognition of internal public key data here. Let's ignore them.
            }
          }
          break;
        case "1.2.840.113549.1.1.1": // RSA
          {
            const publicKeyASN1 = asn1js.fromBER(this.subjectPublicKey.valueBlock.valueHexView);
            if (publicKeyASN1.offset !== -1) {
              try {
                this._parsedKey = new RSAPublicKey({ schema: publicKeyASN1.result });
              }
              catch (ex) {
                // nothing
              } // Could be a problems during recognition of internal public key data here. Let's ignore them.
            }
          }
          break;
        default:
      }

      this._parsedKey ||= null;
    }

    return this._parsedKey || undefined;
  }
  public set parsedKey(value: ECPublicKey | RSAPublicKey | undefined) {
    this._parsedKey = value;
  }

  /**
   * Initializes a new instance of the {@link PublicKeyInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PublicKeyInfoParameters = {}) {
    super();

    this.algorithm = pvutils.getParametersValue(parameters, ALGORITHM, PublicKeyInfo.defaultValues(ALGORITHM));
    this.subjectPublicKey = pvutils.getParametersValue(parameters, SUBJECT_PUBLIC_KEY, PublicKeyInfo.defaultValues(SUBJECT_PUBLIC_KEY));
    const parsedKey = pvutils.getParametersValue(parameters, "parsedKey", null);
    if (parsedKey) {
      this.parsedKey = parsedKey;
    }

    if (parameters.json) {
      this.fromJSON(parameters.json);
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
  public static override defaultValues(memberName: typeof ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SUBJECT_PUBLIC_KEY): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ALGORITHM:
        return new AlgorithmIdentifier();
      case SUBJECT_PUBLIC_KEY:
        return new asn1js.BitString();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SubjectPublicKeyInfo ::= Sequence  {
   *    algorithm            AlgorithmIdentifier,
   *    subjectPublicKey     BIT STRING  }
   *```
   */
  public static override schema(parameters: PublicKeyInfoSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.algorithm || {}),
        new asn1js.BitString({ name: (names.subjectPublicKey || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      PublicKeyInfo.schema({
        names: {
          algorithm: {
            names: {
              blockName: ALGORITHM
            }
          },
          subjectPublicKey: SUBJECT_PUBLIC_KEY
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.algorithm = new AlgorithmIdentifier({ schema: asn1.result.algorithm });
    this.subjectPublicKey = asn1.result.subjectPublicKey;
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    return (new asn1js.Sequence({
      value: [
        this.algorithm.toSchema(),
        this.subjectPublicKey
      ]
    }));
  }

  public toJSON(): PublicKeyInfoJson | JsonWebKey {
    //#region Return common value in case we do not have enough info fo making JWK
    if (!this.parsedKey) {
      return {
        algorithm: this.algorithm.toJSON(),
        subjectPublicKey: this.subjectPublicKey.toJSON(),
      };
    }
    //#endregion

    //#region Making JWK
    const jwk: JsonWebKey = {};

    switch (this.algorithm.algorithmId) {
      case "1.2.840.10045.2.1": // ECDSA
        jwk.kty = "EC";
        break;
      case "1.2.840.113549.1.1.1": // RSA
        jwk.kty = "RSA";
        break;
      default:
    }

    const publicKeyJWK = this.parsedKey.toJSON();
    Object.assign(jwk, publicKeyJWK);

    return jwk;
    //#endregion
  }

  /**
   * Converts JSON value into current object
   * @param json JSON object
   */
  public fromJSON(json: any): void {
    if ("kty" in json) {
      switch (json.kty.toUpperCase()) {
        case "EC":
          this.parsedKey = new ECPublicKey({ json });

          this.algorithm = new AlgorithmIdentifier({
            algorithmId: "1.2.840.10045.2.1",
            algorithmParams: new asn1js.ObjectIdentifier({ value: this.parsedKey.namedCurve })
          });
          break;
        case "RSA":
          this.parsedKey = new RSAPublicKey({ json });

          this.algorithm = new AlgorithmIdentifier({
            algorithmId: "1.2.840.113549.1.1.1",
            algorithmParams: new asn1js.Null()
          });
          break;
        default:
          throw new Error(`Invalid value for "kty" parameter: ${json.kty}`);
      }

      this.subjectPublicKey = new asn1js.BitString({ valueHex: this.parsedKey.toSchema().toBER(false) });
    }
  }

  public async importKey(publicKey: CryptoKey, crypto = common.getCrypto(true)): Promise<void> {
    try {
      if (!publicKey) {
        throw new Error("Need to provide publicKey input parameter");
      }

      const exportedKey = await crypto.exportKey("spki", publicKey);
      const asn1 = asn1js.fromBER(exportedKey);
      try {
        this.fromSchema(asn1.result);
      }
      catch (exception) {
        throw new Error("Error during initializing object from schema");
      }
    } catch (e) {
      const message = e instanceof Error ? e.message : `${e}`;
      throw new Error(`Error during exporting public key: ${message}`);
    }
  }

}
