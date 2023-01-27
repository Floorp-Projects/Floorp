import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { id_ContentType_Data, id_ContentType_EncryptedData, id_ContentType_EnvelopedData, id_ContentType_SignedData } from "./ObjectIdentifiers";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const CONTENT_TYPE = "contentType";
const CONTENT = "content";
const CLEAR_PROPS = [CONTENT_TYPE, CONTENT];

export interface IContentInfo {
  contentType: string;
  content: any;
}

export type ContentInfoParameters = PkiObjectParameters & Partial<IContentInfo>;

export type ContentInfoSchema = Schema.SchemaParameters<{
  contentType?: string;
  content?: string;
}>;

export interface ContentInfoJson {
  contentType: string;
  content?: any;
}

/**
 * Represents the ContentInfo structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class ContentInfo extends PkiObject implements IContentInfo {

  public static override CLASS_NAME = "ContentInfo";
  public static readonly DATA = id_ContentType_Data;
  public static readonly SIGNED_DATA = id_ContentType_SignedData;
  public static readonly ENVELOPED_DATA = id_ContentType_EnvelopedData;
  public static readonly ENCRYPTED_DATA = id_ContentType_EncryptedData;

  public contentType!: string;
  public content: any;

  /**
   * Initializes a new instance of the {@link ContentInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ContentInfoParameters = {}) {
    super();

    this.contentType = pvutils.getParametersValue(parameters, CONTENT_TYPE, ContentInfo.defaultValues(CONTENT_TYPE));
    this.content = pvutils.getParametersValue(parameters, CONTENT, ContentInfo.defaultValues(CONTENT));

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
  public static override defaultValues(memberName: typeof CONTENT): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CONTENT_TYPE:
        return EMPTY_STRING;
      case CONTENT:
        return new asn1js.Any();
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
      case CONTENT_TYPE:
        return (typeof memberValue === "string" &&
          memberValue === this.defaultValues(CONTENT_TYPE));
      case CONTENT:
        return (memberValue instanceof asn1js.Any);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * ContentInfo ::= SEQUENCE {
   *    contentType ContentType,
   *    content [0] EXPLICIT ANY DEFINED BY contentType }
   *```
   */
  public static override schema(parameters: ContentInfoSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    if (("optional" in names) === false) {
      names.optional = false;
    }

    return (new asn1js.Sequence({
      name: (names.blockName || "ContentInfo"),
      optional: names.optional,
      value: [
        new asn1js.ObjectIdentifier({ name: (names.contentType || CONTENT_TYPE) }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Any({ name: (names.content || CONTENT) })] // EXPLICIT ANY value
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
      ContentInfo.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.contentType = asn1.result.contentType.valueBlock.toString();
    this.content = asn1.result.content;
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.contentType }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [this.content] // EXPLICIT ANY value
        })
      ]
    }));
    //#endregion
  }

  public toJSON(): ContentInfoJson {
    const object: ContentInfoJson = {
      contentType: this.contentType
    };

    if (!(this.content instanceof asn1js.Any)) {
      object.content = this.content.toJSON();
    }

    return object;
  }

}
