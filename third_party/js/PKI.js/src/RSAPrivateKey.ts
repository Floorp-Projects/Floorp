import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError, ParameterError } from "./errors";
import { OtherPrimeInfo, OtherPrimeInfoJson, OtherPrimeInfoSchema } from "./OtherPrimeInfo";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const VERSION = "version";
const MODULUS = "modulus";
const PUBLIC_EXPONENT = "publicExponent";
const PRIVATE_EXPONENT = "privateExponent";
const PRIME1 = "prime1";
const PRIME2 = "prime2";
const EXPONENT1 = "exponent1";
const EXPONENT2 = "exponent2";
const COEFFICIENT = "coefficient";
const OTHER_PRIME_INFOS = "otherPrimeInfos";
const CLEAR_PROPS = [
  VERSION,
  MODULUS,
  PUBLIC_EXPONENT,
  PRIVATE_EXPONENT,
  PRIME1,
  PRIME2,
  EXPONENT1,
  EXPONENT2,
  COEFFICIENT,
  OTHER_PRIME_INFOS
];

export interface IRSAPrivateKey {
  version: number;
  modulus: asn1js.Integer;
  publicExponent: asn1js.Integer;
  privateExponent: asn1js.Integer;
  prime1: asn1js.Integer;
  prime2: asn1js.Integer;
  exponent1: asn1js.Integer;
  exponent2: asn1js.Integer;
  coefficient: asn1js.Integer;
  otherPrimeInfos?: OtherPrimeInfo[];
}

export type RSAPrivateKeyParameters = PkiObjectParameters & Partial<IRSAPrivateKey> & { json?: RSAPrivateKeyJson; };

export interface RSAPrivateKeyJson {
  n: string;
  e: string;
  d: string;
  p: string;
  q: string;
  dp: string;
  dq: string;
  qi: string;
  oth?: OtherPrimeInfoJson[];
}

/**
 * Represents the PrivateKeyInfo structure described in [RFC3447](https://datatracker.ietf.org/doc/html/rfc3447)
 */
export class RSAPrivateKey extends PkiObject implements IRSAPrivateKey {

  public static override CLASS_NAME = "RSAPrivateKey";

  public version!: number;
  public modulus!: asn1js.Integer;
  public publicExponent!: asn1js.Integer;
  public privateExponent!: asn1js.Integer;
  public prime1!: asn1js.Integer;
  public prime2!: asn1js.Integer;
  public exponent1!: asn1js.Integer;
  public exponent2!: asn1js.Integer;
  public coefficient!: asn1js.Integer;
  public otherPrimeInfos?: OtherPrimeInfo[];

  /**
   * Initializes a new instance of the {@link RSAPrivateKey} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RSAPrivateKeyParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, RSAPrivateKey.defaultValues(VERSION));
    this.modulus = pvutils.getParametersValue(parameters, MODULUS, RSAPrivateKey.defaultValues(MODULUS));
    this.publicExponent = pvutils.getParametersValue(parameters, PUBLIC_EXPONENT, RSAPrivateKey.defaultValues(PUBLIC_EXPONENT));
    this.privateExponent = pvutils.getParametersValue(parameters, PRIVATE_EXPONENT, RSAPrivateKey.defaultValues(PRIVATE_EXPONENT));
    this.prime1 = pvutils.getParametersValue(parameters, PRIME1, RSAPrivateKey.defaultValues(PRIME1));
    this.prime2 = pvutils.getParametersValue(parameters, PRIME2, RSAPrivateKey.defaultValues(PRIME2));
    this.exponent1 = pvutils.getParametersValue(parameters, EXPONENT1, RSAPrivateKey.defaultValues(EXPONENT1));
    this.exponent2 = pvutils.getParametersValue(parameters, EXPONENT2, RSAPrivateKey.defaultValues(EXPONENT2));
    this.coefficient = pvutils.getParametersValue(parameters, COEFFICIENT, RSAPrivateKey.defaultValues(COEFFICIENT));
    if (OTHER_PRIME_INFOS in parameters) {
      this.otherPrimeInfos = pvutils.getParametersValue(parameters, OTHER_PRIME_INFOS, RSAPrivateKey.defaultValues(OTHER_PRIME_INFOS));
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
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof MODULUS): asn1js.Integer;
  public static override defaultValues(memberName: typeof PUBLIC_EXPONENT): asn1js.Integer;
  public static override defaultValues(memberName: typeof PRIVATE_EXPONENT): asn1js.Integer;
  public static override defaultValues(memberName: typeof PRIME1): asn1js.Integer;
  public static override defaultValues(memberName: typeof PRIME2): asn1js.Integer;
  public static override defaultValues(memberName: typeof EXPONENT1): asn1js.Integer;
  public static override defaultValues(memberName: typeof EXPONENT2): asn1js.Integer;
  public static override defaultValues(memberName: typeof COEFFICIENT): asn1js.Integer;
  public static override defaultValues(memberName: typeof OTHER_PRIME_INFOS): OtherPrimeInfo[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case MODULUS:
        return new asn1js.Integer();
      case PUBLIC_EXPONENT:
        return new asn1js.Integer();
      case PRIVATE_EXPONENT:
        return new asn1js.Integer();
      case PRIME1:
        return new asn1js.Integer();
      case PRIME2:
        return new asn1js.Integer();
      case EXPONENT1:
        return new asn1js.Integer();
      case EXPONENT2:
        return new asn1js.Integer();
      case COEFFICIENT:
        return new asn1js.Integer();
      case OTHER_PRIME_INFOS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RSAPrivateKey ::= Sequence {
   *    version           Version,
   *    modulus           Integer,  -- n
   *    publicExponent    Integer,  -- e
   *    privateExponent   Integer,  -- d
   *    prime1            Integer,  -- p
   *    prime2            Integer,  -- q
   *    exponent1         Integer,  -- d mod (p-1)
   *    exponent2         Integer,  -- d mod (q-1)
   *    coefficient       Integer,  -- (inverse of q) mod p
   *    otherPrimeInfos   OtherPrimeInfos OPTIONAL
   * }
   *
   * OtherPrimeInfos ::= Sequence SIZE(1..MAX) OF OtherPrimeInfo
   * ```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    modulus?: string;
    publicExponent?: string;
    privateExponent?: string;
    prime1?: string;
    prime2?: string;
    exponent1?: string;
    exponent2?: string;
    coefficient?: string;
    otherPrimeInfosName?: string;
    otherPrimeInfo?: OtherPrimeInfoSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.modulus || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.publicExponent || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.privateExponent || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.prime1 || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.prime2 || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.exponent1 || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.exponent2 || EMPTY_STRING) }),
        new asn1js.Integer({ name: (names.coefficient || EMPTY_STRING) }),
        new asn1js.Sequence({
          optional: true,
          value: [
            new asn1js.Repeated({
              name: (names.otherPrimeInfosName || EMPTY_STRING),
              value: OtherPrimeInfo.schema(names.otherPrimeInfo || {})
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
      RSAPrivateKey.schema({
        names: {
          version: VERSION,
          modulus: MODULUS,
          publicExponent: PUBLIC_EXPONENT,
          privateExponent: PRIVATE_EXPONENT,
          prime1: PRIME1,
          prime2: PRIME2,
          exponent1: EXPONENT1,
          exponent2: EXPONENT2,
          coefficient: COEFFICIENT,
          otherPrimeInfo: {
            names: {
              blockName: OTHER_PRIME_INFOS
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;
    this.modulus = asn1.result.modulus.convertFromDER(256);
    this.publicExponent = asn1.result.publicExponent;
    this.privateExponent = asn1.result.privateExponent.convertFromDER(256);
    this.prime1 = asn1.result.prime1.convertFromDER(128);
    this.prime2 = asn1.result.prime2.convertFromDER(128);
    this.exponent1 = asn1.result.exponent1.convertFromDER(128);
    this.exponent2 = asn1.result.exponent2.convertFromDER(128);
    this.coefficient = asn1.result.coefficient.convertFromDER(128);

    if (OTHER_PRIME_INFOS in asn1.result)
      this.otherPrimeInfos = Array.from(asn1.result.otherPrimeInfos, element => new OtherPrimeInfo({ schema: element }));
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(new asn1js.Integer({ value: this.version }));
    outputArray.push(this.modulus.convertToDER());
    outputArray.push(this.publicExponent);
    outputArray.push(this.privateExponent.convertToDER());
    outputArray.push(this.prime1.convertToDER());
    outputArray.push(this.prime2.convertToDER());
    outputArray.push(this.exponent1.convertToDER());
    outputArray.push(this.exponent2.convertToDER());
    outputArray.push(this.coefficient.convertToDER());

    if (this.otherPrimeInfos) {
      outputArray.push(new asn1js.Sequence({
        value: Array.from(this.otherPrimeInfos, o => o.toSchema())
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): RSAPrivateKeyJson {
    const jwk: RSAPrivateKeyJson = {
      n: pvtsutils.Convert.ToBase64Url(this.modulus.valueBlock.valueHexView),
      e: pvtsutils.Convert.ToBase64Url(this.publicExponent.valueBlock.valueHexView),
      d: pvtsutils.Convert.ToBase64Url(this.privateExponent.valueBlock.valueHexView),
      p: pvtsutils.Convert.ToBase64Url(this.prime1.valueBlock.valueHexView),
      q: pvtsutils.Convert.ToBase64Url(this.prime2.valueBlock.valueHexView),
      dp: pvtsutils.Convert.ToBase64Url(this.exponent1.valueBlock.valueHexView),
      dq: pvtsutils.Convert.ToBase64Url(this.exponent2.valueBlock.valueHexView),
      qi: pvtsutils.Convert.ToBase64Url(this.coefficient.valueBlock.valueHexView),
    };
    if (this.otherPrimeInfos) {
      jwk.oth = Array.from(this.otherPrimeInfos, o => o.toJSON());
    }

    return jwk;
  }

  /**
   * Converts JSON value into current object
   * @param json JSON object
   */
  public fromJSON(json: any): void {
    ParameterError.assert("json", json, "n", "e", "d", "p", "q", "dp", "dq", "qi");

    this.modulus = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.n) });
    this.publicExponent = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.e) });
    this.privateExponent = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.d) });
    this.prime1 = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.p) });
    this.prime2 = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.q) });
    this.exponent1 = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.dp) });
    this.exponent2 = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.dq) });
    this.coefficient = new asn1js.Integer({ valueHex: pvtsutils.Convert.FromBase64Url(json.qi) });
    if (json.oth) {
      this.otherPrimeInfos = Array.from(json.oth, (element: any) => new OtherPrimeInfo({ json: element }));
    }
  }

}
