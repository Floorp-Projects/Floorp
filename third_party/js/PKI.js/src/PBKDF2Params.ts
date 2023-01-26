import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const SALT = "salt";
const ITERATION_COUNT = "iterationCount";
const KEY_LENGTH = "keyLength";
const PRF = "prf";
const CLEAR_PROPS = [
  SALT,
  ITERATION_COUNT,
  KEY_LENGTH,
  PRF
];

export interface IPBKDF2Params {
  salt: any;
  iterationCount: number;
  keyLength?: number;
  prf?: AlgorithmIdentifier;
}

export interface PBKDF2ParamsJson {
  salt: any;
  iterationCount: number;
  keyLength?: number;
  prf?: AlgorithmIdentifierJson;
}

export type PBKDF2ParamsParameters = PkiObjectParameters & Partial<IPBKDF2Params>;

/**
 * Represents the PBKDF2Params structure described in [RFC2898](https://www.ietf.org/rfc/rfc2898.txt)
 */
export class PBKDF2Params extends PkiObject implements IPBKDF2Params {

  public static override CLASS_NAME = "PBKDF2Params";

  public salt: any;
  public iterationCount!: number;
  public keyLength?: number;
  public prf?: AlgorithmIdentifier;

  /**
   * Initializes a new instance of the {@link PBKDF2Params} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PBKDF2ParamsParameters = {}) {
    super();

    this.salt = pvutils.getParametersValue(parameters, SALT, PBKDF2Params.defaultValues(SALT));
    this.iterationCount = pvutils.getParametersValue(parameters, ITERATION_COUNT, PBKDF2Params.defaultValues(ITERATION_COUNT));
    if (KEY_LENGTH in parameters) {
      this.keyLength = pvutils.getParametersValue(parameters, KEY_LENGTH, PBKDF2Params.defaultValues(KEY_LENGTH));
    }
    if (PRF in parameters) {
      this.prf = pvutils.getParametersValue(parameters, PRF, PBKDF2Params.defaultValues(PRF));
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
  public static override defaultValues(memberName: typeof SALT): any;
  public static override defaultValues(memberName: typeof ITERATION_COUNT): number;
  public static override defaultValues(memberName: typeof KEY_LENGTH): number;
  public static override defaultValues(memberName: typeof PRF): AlgorithmIdentifier;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case SALT:
        return {};
      case ITERATION_COUNT:
        return (-1);
      case KEY_LENGTH:
        return 0;
      case PRF:
        return new AlgorithmIdentifier({
          algorithmId: "1.3.14.3.2.26", // SHA-1
          algorithmParams: new asn1js.Null()
        });
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PBKDF2-params ::= SEQUENCE {
   *    salt CHOICE {
   *        specified OCTET STRING,
   *        otherSource AlgorithmIdentifier },
   *  iterationCount INTEGER (1..MAX),
   *  keyLength INTEGER (1..MAX) OPTIONAL,
   *  prf AlgorithmIdentifier
   *    DEFAULT { algorithm hMAC-SHA1, parameters NULL } }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    saltPrimitive?: string;
    saltConstructed?: AlgorithmIdentifierSchema;
    iterationCount?: string;
    keyLength?: string;
    prf?: AlgorithmIdentifierSchema;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Choice({
          value: [
            new asn1js.OctetString({ name: (names.saltPrimitive || EMPTY_STRING) }),
            AlgorithmIdentifier.schema(names.saltConstructed || {})
          ]
        }),
        new asn1js.Integer({ name: (names.iterationCount || EMPTY_STRING) }),
        new asn1js.Integer({
          name: (names.keyLength || EMPTY_STRING),
          optional: true
        }),
        AlgorithmIdentifier.schema(names.prf || {
          names: {
            optional: true
          }
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
      PBKDF2Params.schema({
        names: {
          saltPrimitive: SALT,
          saltConstructed: {
            names: {
              blockName: SALT
            }
          },
          iterationCount: ITERATION_COUNT,
          keyLength: KEY_LENGTH,
          prf: {
            names: {
              blockName: PRF,
              optional: true
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.salt = asn1.result.salt;
    this.iterationCount = asn1.result.iterationCount.valueBlock.valueDec;
    if (KEY_LENGTH in asn1.result)
      this.keyLength = asn1.result.keyLength.valueBlock.valueDec;
    if (PRF in asn1.result)
      this.prf = new AlgorithmIdentifier({ schema: asn1.result.prf });
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.salt);
    outputArray.push(new asn1js.Integer({ value: this.iterationCount }));

    if (KEY_LENGTH in this) {
      if (PBKDF2Params.defaultValues(KEY_LENGTH) !== this.keyLength)
        outputArray.push(new asn1js.Integer({ value: this.keyLength }));
    }

    if (this.prf) {
      if (PBKDF2Params.defaultValues(PRF).isEqual(this.prf) === false)
        outputArray.push(this.prf.toSchema());
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): PBKDF2ParamsJson {
    const res: PBKDF2ParamsJson = {
      salt: this.salt.toJSON(),
      iterationCount: this.iterationCount
    };

    if (KEY_LENGTH in this) {
      if (PBKDF2Params.defaultValues(KEY_LENGTH) !== this.keyLength)
        res.keyLength = this.keyLength;
    }

    if (this.prf) {
      if (PBKDF2Params.defaultValues(PRF).isEqual(this.prf) === false)
        res.prf = this.prf.toJSON();
    }

    return res;
  }

}
