import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const DIGEST_ALGORITHM = "digestAlgorithm";
const DIGEST = "digest";
const CLEAR_PROPS = [
  DIGEST_ALGORITHM,
  DIGEST
];

export interface IDigestInfo {
  digestAlgorithm: AlgorithmIdentifier;
  digest: asn1js.OctetString;
}

export interface DigestInfoJson {
  digestAlgorithm: AlgorithmIdentifierJson;
  digest: asn1js.OctetStringJson;
}

export type DigestInfoParameters = PkiObjectParameters & Partial<IDigestInfo>;

export type DigestInfoSchema = Schema.SchemaParameters<{
  digestAlgorithm?: AlgorithmIdentifierSchema;
  digest?: string;
}>;

/**
 * Represents the DigestInfo structure described in [RFC3447](https://datatracker.ietf.org/doc/html/rfc3447)
 */
export class DigestInfo extends PkiObject implements IDigestInfo {

  public static override CLASS_NAME = "DigestInfo";

  public digestAlgorithm!: AlgorithmIdentifier;
  public digest!: asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link DigestInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: DigestInfoParameters = {}) {
    super();

    this.digestAlgorithm = pvutils.getParametersValue(parameters, DIGEST_ALGORITHM, DigestInfo.defaultValues(DIGEST_ALGORITHM));
    this.digest = pvutils.getParametersValue(parameters, DIGEST, DigestInfo.defaultValues(DIGEST));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof DIGEST_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof DIGEST): asn1js.OctetString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case DIGEST_ALGORITHM:
        return new AlgorithmIdentifier();
      case DIGEST:
        return new asn1js.OctetString();
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
      case DIGEST_ALGORITHM:
        return ((AlgorithmIdentifier.compareWithDefault("algorithmId", memberValue.algorithmId)) &&
          (("algorithmParams" in memberValue) === false));
      case DIGEST:
        return (memberValue.isEqual(DigestInfo.defaultValues(memberName)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * DigestInfo ::= SEQUENCE {
   *    digestAlgorithm DigestAlgorithmIdentifier,
   *    digest Digest }
   *
   * Digest ::= OCTET STRING
   *```
   */
  public static override schema(parameters: DigestInfoSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.digestAlgorithm || {
          names: {
            blockName: DIGEST_ALGORITHM
          }
        }),
        new asn1js.OctetString({ name: (names.digest || DIGEST) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      DigestInfo.schema({
        names: {
          digestAlgorithm: {
            names: {
              blockName: DIGEST_ALGORITHM
            }
          },
          digest: DIGEST
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.digestAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.digestAlgorithm });
    this.digest = asn1.result.digest;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        this.digestAlgorithm.toSchema(),
        this.digest
      ]
    }));
  }

  public toJSON(): DigestInfoJson {
    return {
      digestAlgorithm: this.digestAlgorithm.toJSON(),
      digest: this.digest.toJSON(),
    };
  }

}
