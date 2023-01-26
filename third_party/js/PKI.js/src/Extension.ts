import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as Schema from "./Schema";
import { ExtensionParsedValue, ExtensionValueFactory } from "./ExtensionValueFactory";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const EXTN_ID = "extnID";
const CRITICAL = "critical";
const EXTN_VALUE = "extnValue";
const PARSED_VALUE = "parsedValue";
const CLEAR_PROPS = [
  EXTN_ID,
  CRITICAL,
  EXTN_VALUE
];

export interface IExtension {
  extnID: string;
  critical: boolean;
  extnValue: asn1js.OctetString;
  parsedValue?: ExtensionParsedValue;
}

export interface ExtensionConstructorParameters {
  extnID?: string;
  critical?: boolean;
  extnValue?: ArrayBuffer;
  parsedValue?: ExtensionParsedValue;
}

export type ExtensionParameters = PkiObjectParameters & ExtensionConstructorParameters;

export type ExtensionSchema = Schema.SchemaParameters<{
  extnID?: string;
  critical?: string;
  extnValue?: string;
}>;

export interface ExtensionJson {
  extnID: string;
  extnValue: asn1js.OctetStringJson;
  critical?: boolean;
  parsedValue?: any;
}

/**
 * Represents the Extension structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class Extension extends PkiObject implements IExtension {

  public static override CLASS_NAME = "Extension";

  public extnID!: string;
  public critical!: boolean;
  public extnValue!: asn1js.OctetString;

  private _parsedValue?: ExtensionParsedValue | null;
  public get parsedValue(): ExtensionParsedValue | undefined {
    if (this._parsedValue === undefined) {
      // Get PARSED_VALUE for well-known extensions
      const parsedValue = ExtensionValueFactory.fromBER(this.extnID, this.extnValue.valueBlock.valueHexView);
      this._parsedValue = parsedValue;
    }

    return this._parsedValue || undefined;
  }
  public set parsedValue(value: ExtensionParsedValue | undefined) {
    this._parsedValue = value;
  }

  /**
   * Initializes a new instance of the {@link Extension} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ExtensionParameters = {}) {
    super();

    this.extnID = pvutils.getParametersValue(parameters, EXTN_ID, Extension.defaultValues(EXTN_ID));
    this.critical = pvutils.getParametersValue(parameters, CRITICAL, Extension.defaultValues(CRITICAL));
    if (EXTN_VALUE in parameters) {
      this.extnValue = new asn1js.OctetString({ valueHex: parameters.extnValue });
    } else {
      this.extnValue = Extension.defaultValues(EXTN_VALUE);
    }
    if (PARSED_VALUE in parameters) {
      this.parsedValue = pvutils.getParametersValue(parameters, PARSED_VALUE, Extension.defaultValues(PARSED_VALUE));
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
  public static override defaultValues(memberName: typeof EXTN_ID): string;
  public static override defaultValues(memberName: typeof CRITICAL): boolean;
  public static override defaultValues(memberName: typeof EXTN_VALUE): asn1js.OctetString;
  public static override defaultValues(memberName: typeof PARSED_VALUE): ExtensionParsedValue;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case EXTN_ID:
        return EMPTY_STRING;
      case CRITICAL:
        return false;
      case EXTN_VALUE:
        return new asn1js.OctetString();
      case PARSED_VALUE:
        return {};
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * Extension ::= SEQUENCE  {
   *    extnID      OBJECT IDENTIFIER,
   *    critical    BOOLEAN DEFAULT FALSE,
   *    extnValue   OCTET STRING
   * }
   *```
   */
  public static override schema(parameters: ExtensionSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.extnID || EMPTY_STRING) }),
        new asn1js.Boolean({
          name: (names.critical || EMPTY_STRING),
          optional: true
        }),
        new asn1js.OctetString({ name: (names.extnValue || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      Extension.schema({
        names: {
          extnID: EXTN_ID,
          critical: CRITICAL,
          extnValue: EXTN_VALUE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.extnID = asn1.result.extnID.valueBlock.toString();
    if (CRITICAL in asn1.result) {
      this.critical = asn1.result.critical.valueBlock.value;
    }
    this.extnValue = asn1.result.extnValue;
  }

  public toSchema(): asn1js.Sequence {
    // Create array for output sequence
    const outputArray = [];

    outputArray.push(new asn1js.ObjectIdentifier({ value: this.extnID }));

    if (this.critical !== Extension.defaultValues(CRITICAL)) {
      outputArray.push(new asn1js.Boolean({ value: this.critical }));
    }

    outputArray.push(this.extnValue);

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toJSON(): ExtensionJson {
    const object: ExtensionJson = {
      extnID: this.extnID,
      extnValue: this.extnValue.toJSON(),
    };

    if (this.critical !== Extension.defaultValues(CRITICAL)) {
      object.critical = this.critical;
    }
    if (this.parsedValue && this.parsedValue.toJSON) {
      object.parsedValue = this.parsedValue.toJSON();
    }

    return object;
  }

}
