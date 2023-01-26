import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ALGORITHM = "algorithm";
const PUBLIC_KEY = "publicKey";
const CLEAR_PROPS = [
  ALGORITHM,
  PUBLIC_KEY
];

export interface IOriginatorPublicKey {
  algorithm: AlgorithmIdentifier;
  publicKey: asn1js.BitString;
}

export interface OriginatorPublicKeyJson {
  algorithm: AlgorithmIdentifierJson;
  publicKey: asn1js.BitStringJson;
}

export type OriginatorPublicKeyParameters = PkiObjectParameters & Partial<IOriginatorPublicKey>;

/**
 * Represents the OriginatorPublicKey structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class OriginatorPublicKey extends PkiObject implements IOriginatorPublicKey {

  public static override CLASS_NAME = "OriginatorPublicKey";

  public algorithm!: AlgorithmIdentifier;
  public publicKey!: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link OriginatorPublicKey} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OriginatorPublicKeyParameters = {}) {
    super();

    this.algorithm = pvutils.getParametersValue(parameters, ALGORITHM, OriginatorPublicKey.defaultValues(ALGORITHM));
    this.publicKey = pvutils.getParametersValue(parameters, PUBLIC_KEY, OriginatorPublicKey.defaultValues(PUBLIC_KEY));

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
  public static override defaultValues(memberName: typeof PUBLIC_KEY): asn1js.BitString;
  public static override defaultValues(memberName: string): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ALGORITHM:
        return new AlgorithmIdentifier();
      case PUBLIC_KEY:
        return new asn1js.BitString();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compare values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  public static compareWithDefault<T extends { isEqual(data: any): boolean; }>(memberName: string, memberValue: T): memberValue is T {
    switch (memberName) {
      case ALGORITHM:
      case PUBLIC_KEY:
        return (memberValue.isEqual(OriginatorPublicKey.defaultValues(memberName)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * OriginatorPublicKey ::= SEQUENCE {
   *    algorithm AlgorithmIdentifier,
   *    publicKey BIT STRING }
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{
    algorithm?: AlgorithmIdentifierSchema;
    publicKey?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.algorithm || {}),
        new asn1js.BitString({ name: (names.publicKey || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      OriginatorPublicKey.schema({
        names: {
          algorithm: {
            names: {
              blockName: ALGORITHM
            }
          },
          publicKey: PUBLIC_KEY
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.algorithm = new AlgorithmIdentifier({ schema: asn1.result.algorithm });
    this.publicKey = asn1.result.publicKey;
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        this.algorithm.toSchema(),
        this.publicKey
      ]
    }));
    //#endregion
  }

  public toJSON(): OriginatorPublicKeyJson {
    return {
      algorithm: this.algorithm.toJSON(),
      publicKey: this.publicKey.toJSON(),
    };
  }

}
