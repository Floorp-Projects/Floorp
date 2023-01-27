import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { Extension, ExtensionJson, ExtensionSchema } from "./Extension";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const EXTENSIONS = "extensions";
const CLEAR_PROPS = [
  EXTENSIONS,
];

export interface IExtensions {
  /**
   * List of extensions
   */
  extensions: Extension[];
}

export type ExtensionsParameters = PkiObjectParameters & Partial<IExtensions>;

export type ExtensionsSchema = Schema.SchemaParameters<{
  extensions?: string;
  extension?: ExtensionSchema;
}>;

export interface ExtensionsJson {
  extensions: ExtensionJson[];
}

/**
 * Represents the Extensions structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class Extensions extends PkiObject implements IExtensions {

  public static override CLASS_NAME = "Extensions";

  public extensions!: Extension[];

  /**
   * Initializes a new instance of the {@link Extensions} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ExtensionsParameters = {}) {
    super();

    this.extensions = pvutils.getParametersValue(parameters, EXTENSIONS, Extensions.defaultValues(EXTENSIONS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof EXTENSIONS): Extension[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case EXTENSIONS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * Extensions ::= SEQUENCE SIZE (1..MAX) OF Extension
   * ```
   *
   * @param parameters Input parameters for the schema
   * @param optional Flag that current schema should be optional
   * @returns ASN.1 schema object
   */
  public static override schema(parameters: ExtensionsSchema = {}, optional = false) {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      optional,
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.extensions || EMPTY_STRING),
          value: Extension.schema(names.extension || {})
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
      Extensions.schema({
        names: {
          extensions: EXTENSIONS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.extensions = Array.from(asn1.result.extensions, element => new Extension({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.extensions, o => o.toSchema())
    }));
    //#endregion
  }

  public toJSON(): ExtensionsJson {
    return {
      extensions: this.extensions.map(o => o.toJSON())
    };
  }

}
