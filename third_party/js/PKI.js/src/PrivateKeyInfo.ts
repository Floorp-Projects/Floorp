import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { Attribute, AttributeJson } from "./Attribute";
import { EMPTY_STRING } from "./constants";
import { ECPrivateKey } from "./ECPrivateKey";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RSAPrivateKey } from "./RSAPrivateKey";
import * as Schema from "./Schema";

const VERSION = "version";
const PRIVATE_KEY_ALGORITHM = "privateKeyAlgorithm";
const PRIVATE_KEY = "privateKey";
const ATTRIBUTES = "attributes";
const PARSED_KEY = "parsedKey";
const CLEAR_PROPS = [
  VERSION,
  PRIVATE_KEY_ALGORITHM,
  PRIVATE_KEY,
  ATTRIBUTES
];

export interface IPrivateKeyInfo {
  version: number;
  privateKeyAlgorithm: AlgorithmIdentifier;
  privateKey: asn1js.OctetString;
  attributes?: Attribute[];
  parsedKey?: RSAPrivateKey | ECPrivateKey;
}

export type PrivateKeyInfoParameters = PkiObjectParameters & Partial<IPrivateKeyInfo> & { json?: JsonWebKey; };

export interface PrivateKeyInfoJson {
  version: number;
  privateKeyAlgorithm: AlgorithmIdentifierJson;
  privateKey: asn1js.OctetStringJson;
  attributes?: AttributeJson[];
}

/**
 * Represents the PrivateKeyInfo structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5208)
 */
export class PrivateKeyInfo extends PkiObject implements IPrivateKeyInfo {

  public static override CLASS_NAME = "PrivateKeyInfo";

  public version!: number;
  public privateKeyAlgorithm!: AlgorithmIdentifier;
  public privateKey!: asn1js.OctetString;
  public attributes?: Attribute[];
  public parsedKey?: RSAPrivateKey | ECPrivateKey;

  /**
   * Initializes a new instance of the {@link PrivateKeyInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PrivateKeyInfoParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, PrivateKeyInfo.defaultValues(VERSION));
    this.privateKeyAlgorithm = pvutils.getParametersValue(parameters, PRIVATE_KEY_ALGORITHM, PrivateKeyInfo.defaultValues(PRIVATE_KEY_ALGORITHM));
    this.privateKey = pvutils.getParametersValue(parameters, PRIVATE_KEY, PrivateKeyInfo.defaultValues(PRIVATE_KEY));
    if (ATTRIBUTES in parameters) {
      this.attributes = pvutils.getParametersValue(parameters, ATTRIBUTES, PrivateKeyInfo.defaultValues(ATTRIBUTES));
    }
    if (PARSED_KEY in parameters) {
      this.parsedKey = pvutils.getParametersValue(parameters, PARSED_KEY, PrivateKeyInfo.defaultValues(PARSED_KEY));
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
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case PRIVATE_KEY_ALGORITHM:
        return new AlgorithmIdentifier();
      case PRIVATE_KEY:
        return new asn1js.OctetString();
      case ATTRIBUTES:
        return [];
      case PARSED_KEY:
        return {};
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PrivateKeyInfo ::= SEQUENCE {
   *    version Version,
   *    privateKeyAlgorithm AlgorithmIdentifier {{PrivateKeyAlgorithms}},
   *    privateKey PrivateKey,
   *    attributes [0] Attributes OPTIONAL }
   *
   * Version ::= INTEGER {v1(0)} (v1,...)
   *
   * PrivateKey ::= OCTET STRING
   *
   * Attributes ::= SET OF Attribute
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    privateKeyAlgorithm?: AlgorithmIdentifierSchema;
    privateKey?: string;
    attributes?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        AlgorithmIdentifier.schema(names.privateKeyAlgorithm || {}),
        new asn1js.OctetString({ name: (names.privateKey || EMPTY_STRING) }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            new asn1js.Repeated({
              name: (names.attributes || EMPTY_STRING),
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
      PrivateKeyInfo.schema({
        names: {
          version: VERSION,
          privateKeyAlgorithm: {
            names: {
              blockName: PRIVATE_KEY_ALGORITHM
            }
          },
          privateKey: PRIVATE_KEY,
          attributes: ATTRIBUTES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;
    this.privateKeyAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.privateKeyAlgorithm });
    this.privateKey = asn1.result.privateKey;

    if (ATTRIBUTES in asn1.result)
      this.attributes = Array.from(asn1.result.attributes, element => new Attribute({ schema: element }));

    // TODO Use factory
    switch (this.privateKeyAlgorithm.algorithmId) {
      case "1.2.840.113549.1.1.1": // RSA
        {
          const privateKeyASN1 = asn1js.fromBER(this.privateKey.valueBlock.valueHexView);
          if (privateKeyASN1.offset !== -1)
            this.parsedKey = new RSAPrivateKey({ schema: privateKeyASN1.result });
        }
        break;
      case "1.2.840.10045.2.1": // ECDSA
        if ("algorithmParams" in this.privateKeyAlgorithm) {
          if (this.privateKeyAlgorithm.algorithmParams instanceof asn1js.ObjectIdentifier) {
            const privateKeyASN1 = asn1js.fromBER(this.privateKey.valueBlock.valueHexView);
            if (privateKeyASN1.offset !== -1) {
              this.parsedKey = new ECPrivateKey({
                namedCurve: this.privateKeyAlgorithm.algorithmParams.valueBlock.toString(),
                schema: privateKeyASN1.result
              });
            }
          }
        }
        break;
      default:
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray: any = [
      new asn1js.Integer({ value: this.version }),
      this.privateKeyAlgorithm.toSchema(),
      this.privateKey
    ];

    if (this.attributes) {
      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: Array.from(this.attributes, o => o.toSchema())
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): PrivateKeyInfoJson | JsonWebKey {
    //#region Return common value in case we do not have enough info fo making JWK
    if (!this.parsedKey) {
      const object: PrivateKeyInfoJson = {
        version: this.version,
        privateKeyAlgorithm: this.privateKeyAlgorithm.toJSON(),
        privateKey: this.privateKey.toJSON(),
      };

      if (this.attributes) {
        object.attributes = Array.from(this.attributes, o => o.toJSON());
      }

      return object;
    }
    //#endregion

    //#region Making JWK
    const jwk: JsonWebKey = {};

    switch (this.privateKeyAlgorithm.algorithmId) {
      case "1.2.840.10045.2.1": // ECDSA
        jwk.kty = "EC";
        break;
      case "1.2.840.113549.1.1.1": // RSA
        jwk.kty = "RSA";
        break;
      default:
    }

    // TODO Unclear behavior
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
          this.parsedKey = new ECPrivateKey({ json });

          this.privateKeyAlgorithm = new AlgorithmIdentifier({
            algorithmId: "1.2.840.10045.2.1",
            algorithmParams: new asn1js.ObjectIdentifier({ value: this.parsedKey.namedCurve })
          });
          break;
        case "RSA":
          this.parsedKey = new RSAPrivateKey({ json });

          this.privateKeyAlgorithm = new AlgorithmIdentifier({
            algorithmId: "1.2.840.113549.1.1.1",
            algorithmParams: new asn1js.Null()
          });
          break;
        default:
          throw new Error(`Invalid value for "kty" parameter: ${json.kty}`);
      }

      this.privateKey = new asn1js.OctetString({ valueHex: this.parsedKey.toSchema().toBER(false) });
    }
  }

}
