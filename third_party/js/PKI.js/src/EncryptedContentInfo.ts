import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const CONTENT_TYPE = "contentType";
const CONTENT_ENCRYPTION_ALGORITHM = "contentEncryptionAlgorithm";
const ENCRYPTED_CONTENT = "encryptedContent";
const CLEAR_PROPS = [
  CONTENT_TYPE,
  CONTENT_ENCRYPTION_ALGORITHM,
  ENCRYPTED_CONTENT,
];

export interface IEncryptedContentInfo {
  contentType: string;
  contentEncryptionAlgorithm: AlgorithmIdentifier;
  encryptedContent?: asn1js.OctetString;
}

export interface EncryptedContentInfoJson {
  contentType: string;
  contentEncryptionAlgorithm: AlgorithmIdentifierJson;
  encryptedContent?: asn1js.OctetStringJson;
}

export type EncryptedContentParameters = PkiObjectParameters & Partial<IEncryptedContentInfo>;

export type EncryptedContentInfoSchema = Schema.SchemaParameters<{
  contentType?: string;
  contentEncryptionAlgorithm?: AlgorithmIdentifierSchema;
  encryptedContent?: string;
}>;

/**
 * Represents the EncryptedContentInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class EncryptedContentInfo extends PkiObject implements IEncryptedContentInfo {

  public static override CLASS_NAME = "EncryptedContentInfo";

  public contentType!: string;
  public contentEncryptionAlgorithm!: AlgorithmIdentifier;
  public encryptedContent?: asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link EncryptedContent} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: EncryptedContentParameters = {}) {
    super();

    this.contentType = pvutils.getParametersValue(parameters, CONTENT_TYPE, EncryptedContentInfo.defaultValues(CONTENT_TYPE));
    this.contentEncryptionAlgorithm = pvutils.getParametersValue(parameters, CONTENT_ENCRYPTION_ALGORITHM, EncryptedContentInfo.defaultValues(CONTENT_ENCRYPTION_ALGORITHM));

    if (ENCRYPTED_CONTENT in parameters && parameters.encryptedContent) {
      // encryptedContent (!!!) could be constructive or primitive value (!!!)
      this.encryptedContent = parameters.encryptedContent;

      if ((this.encryptedContent.idBlock.tagClass === 1) &&
        (this.encryptedContent.idBlock.tagNumber === 4)) {
        //#region Divide OCTET STRING value down to small pieces
        if (this.encryptedContent.idBlock.isConstructed === false) {
          const constrString = new asn1js.OctetString({
            idBlock: { isConstructed: true },
            isConstructed: true
          });

          let offset = 0;
          const valueHex = this.encryptedContent.valueBlock.valueHexView.slice().buffer;
          let length = valueHex.byteLength;

          const pieceSize = 1024;
          while (length > 0) {
            const pieceView = new Uint8Array(valueHex, offset, ((offset + pieceSize) > valueHex.byteLength) ? (valueHex.byteLength - offset) : pieceSize);
            const _array = new ArrayBuffer(pieceView.length);
            const _view = new Uint8Array(_array);

            for (let i = 0; i < _view.length; i++)
              _view[i] = pieceView[i];

            constrString.valueBlock.value.push(new asn1js.OctetString({ valueHex: _array }));

            length -= pieceView.length;
            offset += pieceView.length;
          }

          this.encryptedContent = constrString;
        }
        //#endregion
      }
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
  public static override defaultValues(memberName: typeof CONTENT_TYPE): string;
  public static override defaultValues(memberName: typeof CONTENT_ENCRYPTION_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ENCRYPTED_CONTENT): asn1js.OctetString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CONTENT_TYPE:
        return EMPTY_STRING;
      case CONTENT_ENCRYPTION_ALGORITHM:
        return new AlgorithmIdentifier();
      case ENCRYPTED_CONTENT:
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
      case CONTENT_TYPE:
        return (memberValue === EMPTY_STRING);
      case CONTENT_ENCRYPTION_ALGORITHM:
        return ((memberValue.algorithmId === EMPTY_STRING) && (("algorithmParams" in memberValue) === false));
      case ENCRYPTED_CONTENT:
        return (memberValue.isEqual(EncryptedContentInfo.defaultValues(ENCRYPTED_CONTENT)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * EncryptedContentInfo ::= SEQUENCE {
   *    contentType ContentType,
   *    contentEncryptionAlgorithm ContentEncryptionAlgorithmIdentifier,
   *    encryptedContent [0] IMPLICIT EncryptedContent OPTIONAL }
   *
   * Comment: Strange, but modern crypto engines create ENCRYPTED_CONTENT as "[0] EXPLICIT EncryptedContent"
   *
   * EncryptedContent ::= OCTET STRING
   *```
   */
  public static override schema(parameters: EncryptedContentInfoSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.contentType || EMPTY_STRING) }),
        AlgorithmIdentifier.schema(names.contentEncryptionAlgorithm || {}),
        // The CHOICE we need because ENCRYPTED_CONTENT could have either "constructive"
        // or "primitive" form of encoding and we need to handle both variants
        new asn1js.Choice({
          value: [
            new asn1js.Constructed({
              name: (names.encryptedContent || EMPTY_STRING),
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 0 // [0]
              },
              value: [
                new asn1js.Repeated({
                  value: new asn1js.OctetString()
                })
              ]
            }),
            new asn1js.Primitive({
              name: (names.encryptedContent || EMPTY_STRING),
              idBlock: {
                tagClass: 3, // CONTEXT-SPECIFIC
                tagNumber: 0 // [0]
              }
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
      EncryptedContentInfo.schema({
        names: {
          contentType: CONTENT_TYPE,
          contentEncryptionAlgorithm: {
            names: {
              blockName: CONTENT_ENCRYPTION_ALGORITHM
            }
          },
          encryptedContent: ENCRYPTED_CONTENT
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.contentType = asn1.result.contentType.valueBlock.toString();
    this.contentEncryptionAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.contentEncryptionAlgorithm });
    if (ENCRYPTED_CONTENT in asn1.result) {
      this.encryptedContent = asn1.result.encryptedContent as asn1js.OctetString;

      this.encryptedContent.idBlock.tagClass = 1; // UNIVERSAL
      this.encryptedContent.idBlock.tagNumber = 4; // OCTET STRING (!!!) The value still has instance of "in_window.org.pkijs.asn1.ASN1_CONSTRUCTED / ASN1_PRIMITIVE"
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const sequenceLengthBlock = {
      isIndefiniteForm: false
    };

    const outputArray = [];

    outputArray.push(new asn1js.ObjectIdentifier({ value: this.contentType }));
    outputArray.push(this.contentEncryptionAlgorithm.toSchema());

    if (this.encryptedContent) {
      sequenceLengthBlock.isIndefiniteForm = this.encryptedContent.idBlock.isConstructed;

      const encryptedValue = this.encryptedContent;

      encryptedValue.idBlock.tagClass = 3; // CONTEXT-SPECIFIC
      encryptedValue.idBlock.tagNumber = 0; // [0]

      encryptedValue.lenBlock.isIndefiniteForm = this.encryptedContent.idBlock.isConstructed;

      outputArray.push(encryptedValue);
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      lenBlock: sequenceLengthBlock,
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): EncryptedContentInfoJson {
    const res: EncryptedContentInfoJson = {
      contentType: this.contentType,
      contentEncryptionAlgorithm: this.contentEncryptionAlgorithm.toJSON()
    };

    if (this.encryptedContent) {
      res.encryptedContent = this.encryptedContent.toJSON();
    }

    return res;
  }

  /**
   * Returns concatenated buffer from `encryptedContent` field.
   * @returns Array buffer
   * @since 3.0.0
   * @throws Throws Error if `encryptedContent` is undefined
   */
  public getEncryptedContent(): ArrayBuffer {
    if (!this.encryptedContent) {
      throw new Error("Parameter 'encryptedContent' is undefined");
    }
    // NOTE encryptedContent can be CONSTRUCTED/PRIMITIVE
    return asn1js.OctetString.prototype.getValue.call(this.encryptedContent);
  }

}

