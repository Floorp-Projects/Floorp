import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "../AlgorithmIdentifier";
import { EMPTY_STRING } from "../constants";
import { AsnError } from "../errors";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import * as Schema from "../Schema";

const DIGESTED_OBJECT_TYPE = "digestedObjectType";
const OTHER_OBJECT_TYPE_ID = "otherObjectTypeID";
const DIGEST_ALGORITHM = "digestAlgorithm";
const OBJECT_DIGEST = "objectDigest";
const CLEAR_PROPS = [
  DIGESTED_OBJECT_TYPE,
  OTHER_OBJECT_TYPE_ID,
  DIGEST_ALGORITHM,
  OBJECT_DIGEST,
];

export interface IObjectDigestInfo {
  digestedObjectType: asn1js.Enumerated;
  otherObjectTypeID?: asn1js.ObjectIdentifier;
  digestAlgorithm: AlgorithmIdentifier;
  objectDigest: asn1js.BitString;
}

export type ObjectDigestInfoParameters = PkiObjectParameters & Partial<IObjectDigestInfo>;

export interface ObjectDigestInfoJson {
  digestedObjectType: asn1js.EnumeratedJson;
  otherObjectTypeID?: asn1js.ObjectIdentifierJson;
  digestAlgorithm: AlgorithmIdentifierJson;
  objectDigest: asn1js.BitStringJson;
}

/**
 * Represents the ObjectDigestInfo structure described in [RFC5755](https://datatracker.ietf.org/doc/html/rfc5755)
 */
export class ObjectDigestInfo extends PkiObject implements IObjectDigestInfo {

  public static override CLASS_NAME = "ObjectDigestInfo";

  public digestedObjectType!: asn1js.Enumerated;
  public otherObjectTypeID?: asn1js.ObjectIdentifier;
  public digestAlgorithm!: AlgorithmIdentifier;
  public objectDigest!: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link ObjectDigestInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ObjectDigestInfoParameters = {}) {
    super();

    this.digestedObjectType = pvutils.getParametersValue(parameters, DIGESTED_OBJECT_TYPE, ObjectDigestInfo.defaultValues(DIGESTED_OBJECT_TYPE));
    if (OTHER_OBJECT_TYPE_ID in parameters) {
      this.otherObjectTypeID = pvutils.getParametersValue(parameters, OTHER_OBJECT_TYPE_ID, ObjectDigestInfo.defaultValues(OTHER_OBJECT_TYPE_ID));
    }
    this.digestAlgorithm = pvutils.getParametersValue(parameters, DIGEST_ALGORITHM, ObjectDigestInfo.defaultValues(DIGEST_ALGORITHM));
    this.objectDigest = pvutils.getParametersValue(parameters, OBJECT_DIGEST, ObjectDigestInfo.defaultValues(OBJECT_DIGEST));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof DIGESTED_OBJECT_TYPE): asn1js.Enumerated;
  public static override defaultValues(memberName: typeof OTHER_OBJECT_TYPE_ID): asn1js.ObjectIdentifier;
  public static override defaultValues(memberName: typeof DIGEST_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof OBJECT_DIGEST): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case DIGESTED_OBJECT_TYPE:
        return new asn1js.Enumerated();
      case OTHER_OBJECT_TYPE_ID:
        return new asn1js.ObjectIdentifier();
      case DIGEST_ALGORITHM:
        return new AlgorithmIdentifier();
      case OBJECT_DIGEST:
        return new asn1js.BitString();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * ObjectDigestInfo ::= SEQUENCE {
   *   digestedObjectType  ENUMERATED {
   *     publicKey            (0),
   *     publicKeyCert        (1),
   *     otherObjectTypes     (2) },
   *   -- otherObjectTypes MUST NOT
   *   -- be used in this profile
   *   otherObjectTypeID   OBJECT IDENTIFIER OPTIONAL,
   *   digestAlgorithm     AlgorithmIdentifier,
   *   objectDigest        BIT STRING
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    digestedObjectType?: string;
    otherObjectTypeID?: string;
    digestAlgorithm?: AlgorithmIdentifierSchema;
    objectDigest?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Enumerated({ name: (names.digestedObjectType || EMPTY_STRING) }),
        new asn1js.ObjectIdentifier({
          optional: true,
          name: (names.otherObjectTypeID || EMPTY_STRING)
        }),
        AlgorithmIdentifier.schema(names.digestAlgorithm || {}),
        new asn1js.BitString({ name: (names.objectDigest || EMPTY_STRING) }),
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      ObjectDigestInfo.schema({
        names: {
          digestedObjectType: DIGESTED_OBJECT_TYPE,
          otherObjectTypeID: OTHER_OBJECT_TYPE_ID,
          digestAlgorithm: {
            names: {
              blockName: DIGEST_ALGORITHM
            }
          },
          objectDigest: OBJECT_DIGEST
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.digestedObjectType = asn1.result.digestedObjectType;

    if (OTHER_OBJECT_TYPE_ID in asn1.result) {
      this.otherObjectTypeID = asn1.result.otherObjectTypeID;
    }

    this.digestAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.digestAlgorithm });
    this.objectDigest = asn1.result.objectDigest;
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    const result = new asn1js.Sequence({
      value: [this.digestedObjectType]
    });

    if (this.otherObjectTypeID) {
      result.valueBlock.value.push(this.otherObjectTypeID);
    }

    result.valueBlock.value.push(this.digestAlgorithm.toSchema());
    result.valueBlock.value.push(this.objectDigest);

    return result;
  }

  public toJSON(): ObjectDigestInfoJson {
    const result: ObjectDigestInfoJson = {
      digestedObjectType: this.digestedObjectType.toJSON(),
      digestAlgorithm: this.digestAlgorithm.toJSON(),
      objectDigest: this.objectDigest.toJSON(),
    };

    if (this.otherObjectTypeID) {
      result.otherObjectTypeID = this.otherObjectTypeID.toJSON();
    }

    return result;
  }

}
