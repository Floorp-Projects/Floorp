import * as asn1js from "asn1js";
import { BufferSourceConverter } from "pvtsutils";
import * as pvutils from "pvutils";
import { EMPTY_BUFFER, EMPTY_STRING } from "./constants";
import { ECNamedCurves } from "./ECNamedCurves";
import { ParameterError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const X = "x";
const Y = "y";
const NAMED_CURVE = "namedCurve";

export interface IECPublicKey {
  namedCurve: string;
  x: ArrayBuffer;
  y: ArrayBuffer;
}

export interface ECPublicKeyJson {
  crv: string;
  x: string;
  y: string;
}

export type ECPublicKeyParameters = PkiObjectParameters & Partial<IECPublicKey> & { json?: ECPublicKeyJson; };

/**
 * Represents the PrivateKeyInfo structure described in [RFC5480](https://datatracker.ietf.org/doc/html/rfc5480)
 */
export class ECPublicKey extends PkiObject implements IECPublicKey {

  public static override CLASS_NAME = "ECPublicKey";

  public namedCurve!: string;
  public x!: ArrayBuffer;
  public y!: ArrayBuffer;

  /**
   * Initializes a new instance of the {@link ECPublicKey} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ECPublicKeyParameters = {}) {
    super();

    this.x = pvutils.getParametersValue(parameters, X, ECPublicKey.defaultValues(X));
    this.y = pvutils.getParametersValue(parameters, Y, ECPublicKey.defaultValues(Y));
    this.namedCurve = pvutils.getParametersValue(parameters, NAMED_CURVE, ECPublicKey.defaultValues(NAMED_CURVE));

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
  public static override defaultValues(memberName: typeof NAMED_CURVE): string;
  public static override defaultValues(memberName: typeof X | typeof Y): ArrayBuffer;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case X:
      case Y:
        return EMPTY_BUFFER;
      case NAMED_CURVE:
        return EMPTY_STRING;
      default:
        return super.defaultValues(memberName);
    }
  }


  /**
   * Compare values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  static compareWithDefault<T>(memberName: string, memberValue: T): memberValue is T {
    switch (memberName) {
      case X:
      case Y:
        return memberValue instanceof ArrayBuffer &&
          (pvutils.isEqualBuffer(memberValue, ECPublicKey.defaultValues(memberName)));
      case NAMED_CURVE:
        return typeof memberValue === "string" &&
          memberValue === ECPublicKey.defaultValues(memberName);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Returns value of pre-defined ASN.1 schema for current class
   * @param parameters Input parameters for the schema
   * @returns ASN.1 schema object
   */
  public static override schema(): Schema.SchemaType {
    return new asn1js.RawData();
  }

  public fromSchema(schema1: BufferSource): any {
    //#region Check the schema is valid

    const view = BufferSourceConverter.toUint8Array(schema1);
    if (view[0] !== 0x04) {
      throw new Error("Object's schema was not verified against input data for ECPublicKey");
    }
    //#endregion

    //#region Get internal properties from parsed schema
    const namedCurve = ECNamedCurves.find(this.namedCurve);
    if (!namedCurve) {
      throw new Error(`Incorrect curve OID: ${this.namedCurve}`);
    }
    const coordinateLength = namedCurve.size;

    if (view.byteLength !== (coordinateLength * 2 + 1)) {
      throw new Error("Object's schema was not verified against input data for ECPublicKey");
    }

    this.namedCurve = namedCurve.name;
    this.x = view.slice(1, coordinateLength + 1).buffer;
    this.y = view.slice(1 + coordinateLength, coordinateLength * 2 + 1).buffer;
    //#endregion
  }

  public toSchema(): asn1js.RawData {
    return new asn1js.RawData({
      data: pvutils.utilConcatBuf(
        (new Uint8Array([0x04])).buffer,
        this.x,
        this.y
      )
    });
  }

  public toJSON(): ECPublicKeyJson {
    const namedCurve = ECNamedCurves.find(this.namedCurve);

    return {
      crv: namedCurve ? namedCurve.name : this.namedCurve,
      x: pvutils.toBase64(pvutils.arrayBufferToString(this.x), true, true, false),
      y: pvutils.toBase64(pvutils.arrayBufferToString(this.y), true, true, false)
    };
  }

  /**
   * Converts JSON value into current object
   * @param json JSON object
   */
  public fromJSON(json: any): void {
    ParameterError.assert("json", json, "crv", "x", "y");

    let coordinateLength = 0;
    const namedCurve = ECNamedCurves.find(json.crv);
    if (namedCurve) {
      this.namedCurve = namedCurve.id;
      coordinateLength = namedCurve.size;
    }

    // TODO Simplify Base64url encoding
    const xConvertBuffer = pvutils.stringToArrayBuffer(pvutils.fromBase64(json.x, true));

    if (xConvertBuffer.byteLength < coordinateLength) {
      this.x = new ArrayBuffer(coordinateLength);
      const view = new Uint8Array(this.x);
      const convertBufferView = new Uint8Array(xConvertBuffer);
      view.set(convertBufferView, 1);
    } else {
      this.x = xConvertBuffer.slice(0, coordinateLength);
    }

    // TODO Simplify Base64url encoding
    const yConvertBuffer = pvutils.stringToArrayBuffer(pvutils.fromBase64(json.y, true));

    if (yConvertBuffer.byteLength < coordinateLength) {
      this.y = new ArrayBuffer(coordinateLength);
      const view = new Uint8Array(this.y);
      const convertBufferView = new Uint8Array(yConvertBuffer);
      view.set(convertBufferView, 1);
    } else {
      this.y = yConvertBuffer.slice(0, coordinateLength);
    }
  }

}
