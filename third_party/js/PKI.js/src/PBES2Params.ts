import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const KEY_DERIVATION_FUNC = "keyDerivationFunc";
const ENCRYPTION_SCHEME = "encryptionScheme";
const CLEAR_PROPS = [
  KEY_DERIVATION_FUNC,
  ENCRYPTION_SCHEME
];

export interface IPBES2Params {
  keyDerivationFunc: AlgorithmIdentifier;
  encryptionScheme: AlgorithmIdentifier;
}

export interface PBES2ParamsJson {
  keyDerivationFunc: AlgorithmIdentifierJson;
  encryptionScheme: AlgorithmIdentifierJson;
}

export type PBES2ParamsParameters = PkiObjectParameters & Partial<IPBES2Params>;

/**
 * Represents the PBES2Params structure described in [RFC2898](https://www.ietf.org/rfc/rfc2898.txt)
 */
export class PBES2Params extends PkiObject implements IPBES2Params {

  public static override CLASS_NAME = "PBES2Params";

  public keyDerivationFunc!: AlgorithmIdentifier;
  public encryptionScheme!: AlgorithmIdentifier;

  /**
   * Initializes a new instance of the {@link PBES2Params} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PBES2ParamsParameters = {}) {
    super();

    this.keyDerivationFunc = pvutils.getParametersValue(parameters, KEY_DERIVATION_FUNC, PBES2Params.defaultValues(KEY_DERIVATION_FUNC));
    this.encryptionScheme = pvutils.getParametersValue(parameters, ENCRYPTION_SCHEME, PBES2Params.defaultValues(ENCRYPTION_SCHEME));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof KEY_DERIVATION_FUNC): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ENCRYPTION_SCHEME): AlgorithmIdentifier;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case KEY_DERIVATION_FUNC:
        return new AlgorithmIdentifier();
      case ENCRYPTION_SCHEME:
        return new AlgorithmIdentifier();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PBES2-params ::= SEQUENCE {
   *    keyDerivationFunc AlgorithmIdentifier {{PBES2-KDFs}},
   *    encryptionScheme AlgorithmIdentifier {{PBES2-Encs}} }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    keyDerivationFunc?: AlgorithmIdentifierSchema;
    encryptionScheme?: AlgorithmIdentifierSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.keyDerivationFunc || {}),
        AlgorithmIdentifier.schema(names.encryptionScheme || {})
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      PBES2Params.schema({
        names: {
          keyDerivationFunc: {
            names: {
              blockName: KEY_DERIVATION_FUNC
            }
          },
          encryptionScheme: {
            names: {
              blockName: ENCRYPTION_SCHEME
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.keyDerivationFunc = new AlgorithmIdentifier({ schema: asn1.result.keyDerivationFunc });
    this.encryptionScheme = new AlgorithmIdentifier({ schema: asn1.result.encryptionScheme });
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        this.keyDerivationFunc.toSchema(),
        this.encryptionScheme.toSchema()
      ]
    }));
  }

  public toJSON(): PBES2ParamsJson {
    return {
      keyDerivationFunc: this.keyDerivationFunc.toJSON(),
      encryptionScheme: this.encryptionScheme.toJSON()
    };
  }

}
