import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { GeneralName, GeneralNameJson } from "./GeneralName";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ALT_NAMES = "altNames";
const CLEAR_PROPS = [
  ALT_NAMES
];

export interface IAltName {
  /**
   * Array of alternative names in GeneralName type
   */
  altNames: GeneralName[];
}

export type AltNameParameters = PkiObjectParameters & Partial<IAltName>;

export interface AltNameJson {
  altNames: GeneralNameJson[];
}

/**
 * Represents the AltName structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class AltName extends PkiObject implements IAltName {

  public static override CLASS_NAME = "AltName";

  public altNames!: GeneralName[];

  /**
   * Initializes a new instance of the {@link AltName} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AltNameParameters = {}) {
    super();

    this.altNames = pvutils.getParametersValue(parameters, ALT_NAMES, AltName.defaultValues(ALT_NAMES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ALT_NAMES): GeneralName[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ALT_NAMES:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AltName ::= GeneralNames
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{ altNames?: string; }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.altNames || EMPTY_STRING),
          value: GeneralName.schema()
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
      AltName.schema({
        names: {
          altNames: ALT_NAMES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (ALT_NAMES in asn1.result) {
      this.altNames = Array.from(asn1.result.altNames, element => new GeneralName({ schema: element }));
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.altNames, o => o.toSchema())
    }));
    //#endregion
  }

  public toJSON(): AltNameJson {
    return {
      altNames: Array.from(this.altNames, o => o.toJSON())
    };
  }

}
