import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";
import * as common from "./common";
import { EMPTY_STRING } from "./constants";

export const HASH_ALGORITHM = "hashAlgorithm";
export const HASHED_MESSAGE = "hashedMessage";
const CLEAR_PROPS = [
  HASH_ALGORITHM,
  HASHED_MESSAGE,
];

export interface IMessageImprint {
  hashAlgorithm: AlgorithmIdentifier;
  hashedMessage: asn1js.OctetString;
}

export interface MessageImprintJson {
  hashAlgorithm: AlgorithmIdentifierJson;
  hashedMessage: asn1js.OctetStringJson;
}

export type MessageImprintParameters = PkiObjectParameters & Partial<IMessageImprint>;

export type MessageImprintSchema = Schema.SchemaParameters<{
  hashAlgorithm?: AlgorithmIdentifierSchema;
  hashedMessage?: string;
}>;

/**
 * Represents the MessageImprint structure described in [RFC3161](https://www.ietf.org/rfc/rfc3161.txt)
 */
export class MessageImprint extends PkiObject implements IMessageImprint {

  public static override CLASS_NAME = "MessageImprint";

  /**
   * Creates and fills a new instance of {@link MessageImprint}
   * @param hashAlgorithm
   * @param message
   * @param crypto Crypto engine
   * @returns New instance of {@link MessageImprint}
   */
  public static async create(hashAlgorithm: string, message: BufferSource, crypto = common.getCrypto(true)): Promise<MessageImprint> {
    const hashAlgorithmOID = crypto.getOIDByAlgorithm({ name: hashAlgorithm }, true, "hashAlgorithm");

    const hashedMessage = await crypto.digest(hashAlgorithm, message);

    const res = new MessageImprint({
      hashAlgorithm: new AlgorithmIdentifier({
        algorithmId: hashAlgorithmOID,
        algorithmParams: new asn1js.Null(),
      }),
      hashedMessage: new asn1js.OctetString({ valueHex: hashedMessage })
    });

    return res;
  }

  public hashAlgorithm!: AlgorithmIdentifier;
  public hashedMessage!: asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link MessageImprint} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: MessageImprintParameters = {}) {
    super();

    this.hashAlgorithm = pvutils.getParametersValue(parameters, HASH_ALGORITHM, MessageImprint.defaultValues(HASH_ALGORITHM));
    this.hashedMessage = pvutils.getParametersValue(parameters, HASHED_MESSAGE, MessageImprint.defaultValues(HASHED_MESSAGE));

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
  public static override defaultValues(memberName: typeof HASHED_MESSAGE): asn1js.OctetString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case HASH_ALGORITHM:
        return new AlgorithmIdentifier();
      case HASHED_MESSAGE:
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
      case HASH_ALGORITHM:
        return ((memberValue.algorithmId === EMPTY_STRING) && (("algorithmParams" in memberValue) === false));
      case HASHED_MESSAGE:
        return (memberValue.isEqual(MessageImprint.defaultValues(memberName)) === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * MessageImprint ::= SEQUENCE  {
   *    hashAlgorithm                AlgorithmIdentifier,
   *    hashedMessage                OCTET STRING  }
   *```
   */
  public static override schema(parameters: MessageImprintSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.hashAlgorithm || {}),
        new asn1js.OctetString({ name: (names.hashedMessage || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      MessageImprint.schema({
        names: {
          hashAlgorithm: {
            names: {
              blockName: HASH_ALGORITHM
            }
          },
          hashedMessage: HASHED_MESSAGE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.hashAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.hashAlgorithm });
    this.hashedMessage = asn1.result.hashedMessage;
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        this.hashAlgorithm.toSchema(),
        this.hashedMessage
      ]
    }));
    //#endregion
  }

  public toJSON(): MessageImprintJson {
    return {
      hashAlgorithm: this.hashAlgorithm.toJSON(),
      hashedMessage: this.hashedMessage.toJSON(),
    };
  }

}

