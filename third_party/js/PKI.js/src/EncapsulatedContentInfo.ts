import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const E_CONTENT_TYPE = "eContentType";
const E_CONTENT = "eContent";
const CLEAR_PROPS = [
  E_CONTENT_TYPE,
  E_CONTENT,
];

export interface IEncapsulatedContentInfo {
  eContentType: string;
  eContent?: asn1js.OctetString;
}

export interface EncapsulatedContentInfoJson {
  eContentType: string;
  eContent?: asn1js.OctetStringJson;
}

export type EncapsulatedContentInfoParameters = PkiObjectParameters & Partial<IEncapsulatedContentInfo>;

export type EncapsulatedContentInfoSchema = Schema.SchemaParameters<{
  eContentType?: string;
  eContent?: string;
}>;

/**
 * Represents the EncapsulatedContentInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class EncapsulatedContentInfo extends PkiObject implements IEncapsulatedContentInfo {

  public static override CLASS_NAME = "EncapsulatedContentInfo";

  public eContentType!: string;
  public eContent?: asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link EncapsulatedContentInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: EncapsulatedContentInfoParameters = {}) {
    super();

    this.eContentType = pvutils.getParametersValue(parameters, E_CONTENT_TYPE, EncapsulatedContentInfo.defaultValues(E_CONTENT_TYPE));
    if (E_CONTENT in parameters) {
      this.eContent = pvutils.getParametersValue(parameters, E_CONTENT, EncapsulatedContentInfo.defaultValues(E_CONTENT));
      if ((this.eContent.idBlock.tagClass === 1) &&
        (this.eContent.idBlock.tagNumber === 4)) {
        //#region Divide OCTET STRING value down to small pieces
        if (this.eContent.idBlock.isConstructed === false) {
          const constrString = new asn1js.OctetString({
            idBlock: { isConstructed: true },
            isConstructed: true
          });

          let offset = 0;
          const viewHex = this.eContent.valueBlock.valueHexView.slice().buffer;
          let length = viewHex.byteLength;

          while (length > 0) {
            const pieceView = new Uint8Array(viewHex, offset, ((offset + 65536) > viewHex.byteLength) ? (viewHex.byteLength - offset) : 65536);
            const _array = new ArrayBuffer(pieceView.length);
            const _view = new Uint8Array(_array);

            for (let i = 0; i < _view.length; i++) {
              _view[i] = pieceView[i];
            }

            constrString.valueBlock.value.push(new asn1js.OctetString({ valueHex: _array }));

            length -= pieceView.length;
            offset += pieceView.length;
          }

          this.eContent = constrString;
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
  public static override defaultValues(memberName: typeof E_CONTENT_TYPE): string;
  public static override defaultValues(memberName: typeof E_CONTENT): asn1js.OctetString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case E_CONTENT_TYPE:
        return EMPTY_STRING;
      case E_CONTENT:
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
      case E_CONTENT_TYPE:
        return (memberValue === EMPTY_STRING);
      case E_CONTENT:
        {
          if ((memberValue.idBlock.tagClass === 1) && (memberValue.idBlock.tagNumber === 4))
            return (memberValue.isEqual(EncapsulatedContentInfo.defaultValues(E_CONTENT)));

          return false;
        }
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * EncapsulatedContentInfo ::= SEQUENCE {
   *    eContentType ContentType,
   *    eContent [0] EXPLICIT OCTET STRING OPTIONAL } * Changed it to ANY, as in PKCS#7
   *```
   */
  public static override schema(parameters: EncapsulatedContentInfoSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.eContentType || EMPTY_STRING) }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            new asn1js.Any({ name: (names.eContent || EMPTY_STRING) }) // In order to aling this with PKCS#7 and CMS as well
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
      EncapsulatedContentInfo.schema({
        names: {
          eContentType: E_CONTENT_TYPE,
          eContent: E_CONTENT
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.eContentType = asn1.result.eContentType.valueBlock.toString();
    if (E_CONTENT in asn1.result)
      this.eContent = asn1.result.eContent;
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(new asn1js.ObjectIdentifier({ value: this.eContentType }));
    if (this.eContent) {
      if (EncapsulatedContentInfo.compareWithDefault(E_CONTENT, this.eContent) === false) {
        outputArray.push(new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [this.eContent]
        }));
      }
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): EncapsulatedContentInfoJson {
    const res: EncapsulatedContentInfoJson = {
      eContentType: this.eContentType
    };

    if (this.eContent && EncapsulatedContentInfo.compareWithDefault(E_CONTENT, this.eContent) === false) {
      res.eContent = this.eContent.toJSON();
    }

    return res;
  }

}

