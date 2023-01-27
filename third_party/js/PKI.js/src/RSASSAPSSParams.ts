import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const HASH_ALGORITHM = "hashAlgorithm";
const MASK_GEN_ALGORITHM = "maskGenAlgorithm";
const SALT_LENGTH = "saltLength";
const TRAILER_FIELD = "trailerField";
const CLEAR_PROPS = [
  HASH_ALGORITHM,
  MASK_GEN_ALGORITHM,
  SALT_LENGTH,
  TRAILER_FIELD
];

export interface IRSASSAPSSParams {
  /**
   * Algorithms of hashing (DEFAULT sha1)
   */
  hashAlgorithm: AlgorithmIdentifier;
  /**
   * Salt length (DEFAULT 20)
   */
  maskGenAlgorithm: AlgorithmIdentifier;
  /**
   * Salt length (DEFAULT 20)
   */
  saltLength: number;
  /**
   * (DEFAULT 1)
   */
  trailerField: number;
}

export interface RSASSAPSSParamsJson {
  hashAlgorithm?: AlgorithmIdentifierJson;
  maskGenAlgorithm?: AlgorithmIdentifierJson;
  saltLength?: number;
  trailerField?: number;
}

export type RSASSAPSSParamsParameters = PkiObjectParameters & Partial<IRSASSAPSSParams>;

/**
 * Represents the RSASSAPSSParams structure described in [RFC4055](https://datatracker.ietf.org/doc/html/rfc4055)
 */
export class RSASSAPSSParams extends PkiObject implements IRSASSAPSSParams {

  public static override CLASS_NAME = "RSASSAPSSParams";

  public hashAlgorithm!: AlgorithmIdentifier;
  public maskGenAlgorithm!: AlgorithmIdentifier;
  public saltLength!: number;
  public trailerField!: number;

  /**
   * Initializes a new instance of the {@link RSASSAPSSParams} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RSASSAPSSParamsParameters = {}) {
    super();

    this.hashAlgorithm = pvutils.getParametersValue(parameters, HASH_ALGORITHM, RSASSAPSSParams.defaultValues(HASH_ALGORITHM));
    this.maskGenAlgorithm = pvutils.getParametersValue(parameters, MASK_GEN_ALGORITHM, RSASSAPSSParams.defaultValues(MASK_GEN_ALGORITHM));
    this.saltLength = pvutils.getParametersValue(parameters, SALT_LENGTH, RSASSAPSSParams.defaultValues(SALT_LENGTH));
    this.trailerField = pvutils.getParametersValue(parameters, TRAILER_FIELD, RSASSAPSSParams.defaultValues(TRAILER_FIELD));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof HASH_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof MASK_GEN_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SALT_LENGTH): number;
  public static override defaultValues(memberName: typeof TRAILER_FIELD): number;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case HASH_ALGORITHM:
        return new AlgorithmIdentifier({
          algorithmId: "1.3.14.3.2.26", // SHA-1
          algorithmParams: new asn1js.Null()
        });
      case MASK_GEN_ALGORITHM:
        return new AlgorithmIdentifier({
          algorithmId: "1.2.840.113549.1.1.8", // MGF1
          algorithmParams: (new AlgorithmIdentifier({
            algorithmId: "1.3.14.3.2.26", // SHA-1
            algorithmParams: new asn1js.Null()
          })).toSchema()
        });
      case SALT_LENGTH:
        return 20;
      case TRAILER_FIELD:
        return 1;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RSASSA-PSS-params ::= Sequence  {
   *    hashAlgorithm      [0] HashAlgorithm DEFAULT sha1Identifier,
   *    maskGenAlgorithm   [1] MaskGenAlgorithm DEFAULT mgf1SHA1Identifier,
   *    saltLength         [2] Integer DEFAULT 20,
   *    trailerField       [3] Integer DEFAULT 1  }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    hashAlgorithm?: AlgorithmIdentifierSchema;
    maskGenAlgorithm?: AlgorithmIdentifierSchema;
    saltLength?: string;
    trailerField?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          optional: true,
          value: [AlgorithmIdentifier.schema(names.hashAlgorithm || {})]
        }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          optional: true,
          value: [AlgorithmIdentifier.schema(names.maskGenAlgorithm || {})]
        }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          },
          optional: true,
          value: [new asn1js.Integer({ name: (names.saltLength || EMPTY_STRING) })]
        }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 3 // [3]
          },
          optional: true,
          value: [new asn1js.Integer({ name: (names.trailerField || EMPTY_STRING) })]
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
      RSASSAPSSParams.schema({
        names: {
          hashAlgorithm: {
            names: {
              blockName: HASH_ALGORITHM
            }
          },
          maskGenAlgorithm: {
            names: {
              blockName: MASK_GEN_ALGORITHM
            }
          },
          saltLength: SALT_LENGTH,
          trailerField: TRAILER_FIELD
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (HASH_ALGORITHM in asn1.result)
      this.hashAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.hashAlgorithm });

    if (MASK_GEN_ALGORITHM in asn1.result)
      this.maskGenAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.maskGenAlgorithm });

    if (SALT_LENGTH in asn1.result)
      this.saltLength = asn1.result.saltLength.valueBlock.valueDec;

    if (TRAILER_FIELD in asn1.result)
      this.trailerField = asn1.result.trailerField.valueBlock.valueDec;
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (!this.hashAlgorithm.isEqual(RSASSAPSSParams.defaultValues(HASH_ALGORITHM))) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [this.hashAlgorithm.toSchema()]
      }));
    }

    if (!this.maskGenAlgorithm.isEqual(RSASSAPSSParams.defaultValues(MASK_GEN_ALGORITHM))) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: [this.maskGenAlgorithm.toSchema()]
      }));
    }

    if (this.saltLength !== RSASSAPSSParams.defaultValues(SALT_LENGTH)) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 2 // [2]
        },
        value: [new asn1js.Integer({ value: this.saltLength })]
      }));
    }

    if (this.trailerField !== RSASSAPSSParams.defaultValues(TRAILER_FIELD)) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 3 // [3]
        },
        value: [new asn1js.Integer({ value: this.trailerField })]
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): RSASSAPSSParamsJson {
    const res: RSASSAPSSParamsJson = {};

    if (!this.hashAlgorithm.isEqual(RSASSAPSSParams.defaultValues(HASH_ALGORITHM))) {
      res.hashAlgorithm = this.hashAlgorithm.toJSON();
    }

    if (!this.maskGenAlgorithm.isEqual(RSASSAPSSParams.defaultValues(MASK_GEN_ALGORITHM))) {
      res.maskGenAlgorithm = this.maskGenAlgorithm.toJSON();
    }

    if (this.saltLength !== RSASSAPSSParams.defaultValues(SALT_LENGTH)) {
      res.saltLength = this.saltLength;
    }

    if (this.trailerField !== RSASSAPSSParams.defaultValues(TRAILER_FIELD)) {
      res.trailerField = this.trailerField;
    }

    return res;
  }

}
