import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { GeneralName, GeneralNameJson, GeneralNameSchema } from "./GeneralName";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const BASE = "base";
const MINIMUM = "minimum";
const MAXIMUM = "maximum";
const CLEAR_PROPS = [
  BASE,
  MINIMUM,
  MAXIMUM
];

export interface IGeneralSubtree {
  base: GeneralName;
  minimum: number | asn1js.Integer;
  maximum?: number | asn1js.Integer;
}

export interface GeneralSubtreeJson {
  base: GeneralNameJson;
  minimum?: number | asn1js.IntegerJson;
  maximum?: number | asn1js.IntegerJson;
}

export type GeneralSubtreeParameters = PkiObjectParameters & Partial<IGeneralSubtree>;

/**
 * Represents the GeneralSubtree structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class GeneralSubtree extends PkiObject implements IGeneralSubtree {

  public static override CLASS_NAME = "GeneralSubtree";

  public base!: GeneralName;
  public minimum!: number | asn1js.Integer;
  public maximum?: number | asn1js.Integer;

  /**
   * Initializes a new instance of the {@link GeneralSubtree} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: GeneralSubtreeParameters = {}) {
    super();

    this.base = pvutils.getParametersValue(parameters, BASE, GeneralSubtree.defaultValues(BASE));
    this.minimum = pvutils.getParametersValue(parameters, MINIMUM, GeneralSubtree.defaultValues(MINIMUM));
    if (MAXIMUM in parameters) {
      this.maximum = pvutils.getParametersValue(parameters, MAXIMUM, GeneralSubtree.defaultValues(MAXIMUM));
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
  public static override defaultValues(memberName: typeof BASE): GeneralName;
  public static override defaultValues(memberName: typeof MINIMUM): number;
  public static override defaultValues(memberName: typeof MAXIMUM): number;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case BASE:
        return new GeneralName();
      case MINIMUM:
        return 0;
      case MAXIMUM:
        return 0;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * GeneralSubtree ::= SEQUENCE {
   *    base                    GeneralName,
   *    minimum         [0]     BaseDistance DEFAULT 0,
   *    maximum         [1]     BaseDistance OPTIONAL }
   *
   * BaseDistance ::= INTEGER (0..MAX)
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    base?: GeneralNameSchema;
    minimum?: string;
    maximum?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        GeneralName.schema(names.base || {}),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Integer({ name: (names.minimum || EMPTY_STRING) })]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [new asn1js.Integer({ name: (names.maximum || EMPTY_STRING) })]
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
      GeneralSubtree.schema({
        names: {
          base: {
            names: {
              blockName: BASE
            }
          },
          minimum: MINIMUM,
          maximum: MAXIMUM
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.base = new GeneralName({ schema: asn1.result.base });

    if (MINIMUM in asn1.result) {
      if (asn1.result.minimum.valueBlock.isHexOnly)
        this.minimum = asn1.result.minimum;
      else
        this.minimum = asn1.result.minimum.valueBlock.valueDec;
    }

    if (MAXIMUM in asn1.result) {
      if (asn1.result.maximum.valueBlock.isHexOnly)
        this.maximum = asn1.result.maximum;
      else
        this.maximum = asn1.result.maximum.valueBlock.valueDec;
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    outputArray.push(this.base.toSchema());

    if (this.minimum !== 0) {
      let valueMinimum: number | asn1js.Integer = 0;

      if (this.minimum instanceof asn1js.Integer) {
        valueMinimum = this.minimum;
      } else {
        valueMinimum = new asn1js.Integer({ value: this.minimum });
      }

      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [valueMinimum]
      }));
    }

    if (MAXIMUM in this) {
      let valueMaximum: number | asn1js.Integer = 0;

      if (this.maximum instanceof asn1js.Integer) {
        valueMaximum = this.maximum;
      } else {
        valueMaximum = new asn1js.Integer({ value: this.maximum });
      }

      outputArray.push(new asn1js.Constructed({
        optional: true,
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: [valueMaximum]
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): GeneralSubtreeJson {
    const res: GeneralSubtreeJson = {
      base: this.base.toJSON()
    };

    if (this.minimum !== 0) {
      if (typeof this.minimum === "number") {
        res.minimum = this.minimum;
      } else {
        res.minimum = this.minimum.toJSON();
      }
    }

    if (this.maximum !== undefined) {
      if (typeof this.maximum === "number") {
        res.maximum = this.maximum;
      } else {
        res.maximum = this.maximum.toJSON();
      }
    }

    return res;
  }

}
