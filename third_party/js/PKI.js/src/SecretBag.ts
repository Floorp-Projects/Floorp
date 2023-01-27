import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const SECRET_TYPE_ID = "secretTypeId";
const SECRET_VALUE = "secretValue";
const CLEAR_PROPS = [
  SECRET_TYPE_ID,
  SECRET_VALUE,
];

export interface ISecretBag {
  secretTypeId: string;
  secretValue: Schema.SchemaCompatible;
}

export interface SecretBagJson {
  secretTypeId: string;
  secretValue: asn1js.BaseBlockJson;
}

export type SecretBagParameters = PkiObjectParameters & Partial<ISecretBag>;

/**
 * Represents the SecretBag structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class SecretBag extends PkiObject implements ISecretBag {

  public static override CLASS_NAME = "SecretBag";

  public secretTypeId!: string;
  public secretValue!: Schema.SchemaCompatible;

  /**
   * Initializes a new instance of the {@link SecretBag} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SecretBagParameters = {}) {
    super();

    this.secretTypeId = pvutils.getParametersValue(parameters, SECRET_TYPE_ID, SecretBag.defaultValues(SECRET_TYPE_ID));
    this.secretValue = pvutils.getParametersValue(parameters, SECRET_VALUE, SecretBag.defaultValues(SECRET_VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof SECRET_TYPE_ID): string;
  public static override defaultValues(memberName: typeof SECRET_VALUE): Schema.SchemaCompatible;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case SECRET_TYPE_ID:
        return EMPTY_STRING;
      case SECRET_VALUE:
        return (new asn1js.Any());
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
      case SECRET_TYPE_ID:
        return (memberValue === EMPTY_STRING);
      case SECRET_VALUE:
        return (memberValue instanceof asn1js.Any);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SecretBag ::= SEQUENCE {
   *    secretTypeId BAG-TYPE.&id ({SecretTypes}),
   *    secretValue  [0] EXPLICIT BAG-TYPE.&Type ({SecretTypes}{@secretTypeId})
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    id?: string;
    value?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.id || "id") }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Any({ name: (names.value || "value") })] // EXPLICIT ANY value
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
      SecretBag.schema({
        names: {
          id: SECRET_TYPE_ID,
          value: SECRET_VALUE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.secretTypeId = asn1.result.secretTypeId.valueBlock.toString();
    this.secretValue = asn1.result.secretValue;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.secretTypeId }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [this.secretValue.toSchema()]
        })
      ]
    }));
  }

  public toJSON(): SecretBagJson {
    return {
      secretTypeId: this.secretTypeId,
      secretValue: this.secretValue.toJSON()
    };
  }

}
