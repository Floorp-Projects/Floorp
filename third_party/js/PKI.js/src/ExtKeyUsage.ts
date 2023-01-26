import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const KEY_PURPOSES = "keyPurposes";
const CLEAR_PROPS = [
  KEY_PURPOSES,
];

export interface IExtKeyUsage {
  keyPurposes: string[];
}

export interface ExtKeyUsageJson {
  keyPurposes: string[];
}

export type ExtKeyUsageParameters = PkiObjectParameters & Partial<IExtKeyUsage>;

/**
 * Represents the ExtKeyUsage structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class ExtKeyUsage extends PkiObject implements IExtKeyUsage {

  public static override CLASS_NAME = "ExtKeyUsage";

  public keyPurposes!: string[];

  /**
   * Initializes a new instance of the {@link ExtKeyUsage} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ExtKeyUsageParameters = {}) {
    super();

    this.keyPurposes = pvutils.getParametersValue(parameters, KEY_PURPOSES, ExtKeyUsage.defaultValues(KEY_PURPOSES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof KEY_PURPOSES): string[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case KEY_PURPOSES:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * ExtKeyUsage ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
   *
   * KeyPurposeId ::= OBJECT IDENTIFIER
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    keyPurposes?: string;
  }> = {}): Schema.SchemaType {
    /**
     * @type {Object}
     * @property {string} [blockName]
     * @property {string} [keyPurposes]
     */
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.keyPurposes || EMPTY_STRING),
          value: new asn1js.ObjectIdentifier()
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      ExtKeyUsage.schema({
        names: {
          keyPurposes: KEY_PURPOSES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.keyPurposes = Array.from(asn1.result.keyPurposes, (element: asn1js.ObjectIdentifier) => element.valueBlock.toString());
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.keyPurposes, element => new asn1js.ObjectIdentifier({ value: element }))
    }));
  }

  public toJSON(): ExtKeyUsageJson {
    return {
      keyPurposes: Array.from(this.keyPurposes)
    };
  }

}

