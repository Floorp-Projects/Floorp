import * as asn1js from "asn1js";
import * as pvtsutils from "pvtsutils";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { ECNamedCurves } from "./ECNamedCurves";
import { ECPublicKey, ECPublicKeyParameters } from "./ECPublicKey";
import { AsnError, ParameterError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const VERSION = "version";
const PRIVATE_KEY = "privateKey";
const NAMED_CURVE = "namedCurve";
const PUBLIC_KEY = "publicKey";
const CLEAR_PROPS = [
  VERSION,
  PRIVATE_KEY,
  NAMED_CURVE,
  PUBLIC_KEY
];

export interface IECPrivateKey {
  version: number;
  privateKey: asn1js.OctetString;
  namedCurve?: string;
  publicKey?: ECPublicKey;
}

export type ECPrivateKeyParameters = PkiObjectParameters & Partial<IECPrivateKey> & { json?: ECPrivateKeyJson; };

export interface ECPrivateKeyJson {
  crv: string;
  y?: string;
  x?: string;
  d: string;
}

/**
 * Represents the PrivateKeyInfo structure described in [RFC5915](https://datatracker.ietf.org/doc/html/rfc5915)
 */
export class ECPrivateKey extends PkiObject implements IECPrivateKey {

  public static override CLASS_NAME = "ECPrivateKey";

  public version!: number;
  public privateKey!: asn1js.OctetString;
  public namedCurve?: string;
  public publicKey?: ECPublicKey;

  /**
   * Initializes a new instance of the {@link ECPrivateKey} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ECPrivateKeyParameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, ECPrivateKey.defaultValues(VERSION));
    this.privateKey = pvutils.getParametersValue(parameters, PRIVATE_KEY, ECPrivateKey.defaultValues(PRIVATE_KEY));
    if (NAMED_CURVE in parameters) {
      this.namedCurve = pvutils.getParametersValue(parameters, NAMED_CURVE, ECPrivateKey.defaultValues(NAMED_CURVE));
    }
    if (PUBLIC_KEY in parameters) {
      this.publicKey = pvutils.getParametersValue(parameters, PUBLIC_KEY, ECPrivateKey.defaultValues(PUBLIC_KEY));
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
  public static override defaultValues(memberName: typeof VERSION): 1;
  public static override defaultValues(memberName: typeof PRIVATE_KEY): asn1js.OctetString;
  public static override defaultValues(memberName: typeof NAMED_CURVE): string;
  public static override defaultValues(memberName: typeof PUBLIC_KEY): ECPublicKey;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 1;
      case PRIVATE_KEY:
        return new asn1js.OctetString();
      case NAMED_CURVE:
        return EMPTY_STRING;
      case PUBLIC_KEY:
        return new ECPublicKey();
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
        return (memberValue === ECPrivateKey.defaultValues(memberName));
      case PRIVATE_KEY:
        return (memberValue.isEqual(ECPrivateKey.defaultValues(memberName)));
      case NAMED_CURVE:
        return (memberValue === EMPTY_STRING);
      case PUBLIC_KEY:
        return ((ECPublicKey.compareWithDefault(NAMED_CURVE, memberValue.namedCurve)) &&
          (ECPublicKey.compareWithDefault("x", memberValue.x)) &&
          (ECPublicKey.compareWithDefault("y", memberValue.y)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * ECPrivateKey ::= SEQUENCE {
   * version        INTEGER { ecPrivkeyVer1(1) } (ecPrivkeyVer1),
   * privateKey     OCTET STRING,
   * parameters [0] ECParameters {{ NamedCurve }} OPTIONAL,
   * publicKey  [1] BIT STRING OPTIONAL
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    version?: string;
    privateKey?: string;
    namedCurve?: string;
    publicKey?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        new asn1js.OctetString({ name: (names.privateKey || EMPTY_STRING) }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            new asn1js.ObjectIdentifier({ name: (names.namedCurve || EMPTY_STRING) })
          ]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [
            new asn1js.BitString({ name: (names.publicKey || EMPTY_STRING) })
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
      ECPrivateKey.schema({
        names: {
          version: VERSION,
          privateKey: PRIVATE_KEY,
          namedCurve: NAMED_CURVE,
          publicKey: PUBLIC_KEY
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;
    this.privateKey = asn1.result.privateKey;

    if (NAMED_CURVE in asn1.result) {
      this.namedCurve = asn1.result.namedCurve.valueBlock.toString();
    }

    if (PUBLIC_KEY in asn1.result) {
      const publicKeyData: ECPublicKeyParameters = { schema: asn1.result.publicKey.valueBlock.valueHex };
      if (NAMED_CURVE in this) {
        publicKeyData.namedCurve = this.namedCurve;
      }

      this.publicKey = new ECPublicKey(publicKeyData);
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    const outputArray: any = [
      new asn1js.Integer({ value: this.version }),
      this.privateKey
    ];

    if (this.namedCurve) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [
          new asn1js.ObjectIdentifier({ value: this.namedCurve })
        ]
      }));
    }

    if (this.publicKey) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: [
          new asn1js.BitString({ valueHex: this.publicKey.toSchema().toBER(false) })
        ]
      }));
    }

    return new asn1js.Sequence({
      value: outputArray
    });
  }

  public toJSON(): ECPrivateKeyJson {
    if (!this.namedCurve || ECPrivateKey.compareWithDefault(NAMED_CURVE, this.namedCurve)) {
      throw new Error("Not enough information for making JSON: absent \"namedCurve\" value");
    }

    const curve = ECNamedCurves.find(this.namedCurve);

    const privateKeyJSON: ECPrivateKeyJson = {
      crv: curve ? curve.name : this.namedCurve,
      d: pvtsutils.Convert.ToBase64Url(this.privateKey.valueBlock.valueHexView),
    };

    if (this.publicKey) {
      const publicKeyJSON = this.publicKey.toJSON();

      privateKeyJSON.x = publicKeyJSON.x;
      privateKeyJSON.y = publicKeyJSON.y;
    }

    return privateKeyJSON;
  }

  /**
   * Converts JSON value into current object
   * @param json JSON object
   */
  public fromJSON(json: any): void {
    ParameterError.assert("json", json, "crv", "d");

    let coordinateLength = 0;

    const curve = ECNamedCurves.find(json.crv);
    if (curve) {
      this.namedCurve = curve.id;
      coordinateLength = curve.size;
    }

    const convertBuffer = pvtsutils.Convert.FromBase64Url(json.d);

    if (convertBuffer.byteLength < coordinateLength) {
      const buffer = new ArrayBuffer(coordinateLength);
      const view = new Uint8Array(buffer);
      const convertBufferView = new Uint8Array(convertBuffer);
      view.set(convertBufferView, 1);

      this.privateKey = new asn1js.OctetString({ valueHex: buffer });
    } else {
      this.privateKey = new asn1js.OctetString({ valueHex: convertBuffer.slice(0, coordinateLength) });
    }

    if (json.x && json.y) {
      this.publicKey = new ECPublicKey({ json });
    }
  }

}
