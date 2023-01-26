import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { Attribute, AttributeJson } from "./Attribute";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";

const BAG_ID = "bagId";
const BAG_VALUE = "bagValue";
const BAG_ATTRIBUTES = "bagAttributes";
const CLEAR_PROPS = [
  BAG_ID,
  BAG_VALUE,
  BAG_ATTRIBUTES
];

export interface ISafeBag<T extends BagType = BagType> {
  bagId: string;
  bagValue: T;
  bagAttributes?: Attribute[];
}

export type SafeBagParameters<T extends BagType = BagType> = PkiObjectParameters & Partial<ISafeBag<T>>;

export interface SafeBagJson {
  bagId: string;
  bagValue: BagTypeJson;
  bagAttributes?: AttributeJson[];
}

/**
 * Represents the SafeBag structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class SafeBag<T extends BagType = BagType> extends PkiObject implements ISafeBag<T> {

  public static override CLASS_NAME = "SafeBag";

  public bagId!: string;
  public bagValue!: T;
  public bagAttributes?: Attribute[];

  /**
   * Initializes a new instance of the {@link SafeBag} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SafeBagParameters<T> = {}) {
    super();

    this.bagId = pvutils.getParametersValue(parameters, BAG_ID, SafeBag.defaultValues(BAG_ID));
    this.bagValue = pvutils.getParametersValue(parameters, BAG_VALUE, SafeBag.defaultValues(BAG_VALUE)) as unknown as T;
    if (BAG_ATTRIBUTES in parameters) {
      this.bagAttributes = pvutils.getParametersValue(parameters, BAG_ATTRIBUTES, SafeBag.defaultValues(BAG_ATTRIBUTES));
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
  public static override defaultValues(memberName: typeof BAG_ID): string;
  public static override defaultValues(memberName: typeof BAG_VALUE): BagType;
  public static override defaultValues(memberName: typeof BAG_ATTRIBUTES): Attribute[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case BAG_ID:
        return EMPTY_STRING;
      case BAG_VALUE:
        return (new asn1js.Any());
      case BAG_ATTRIBUTES:
        return [];
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
      case BAG_ID:
        return (memberValue === EMPTY_STRING);
      case BAG_VALUE:
        return (memberValue instanceof asn1js.Any);
      case BAG_ATTRIBUTES:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SafeBag ::= SEQUENCE {
   *    bagId         BAG-TYPE.&id ({PKCS12BagSet}),
   *    bagValue      [0] EXPLICIT BAG-TYPE.&Type({PKCS12BagSet}{@bagId}),
   *    bagAttributes SET OF PKCS12Attribute OPTIONAL
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    bagId?: string;
    bagValue?: string;
    bagAttributes?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.bagId || BAG_ID) }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Any({ name: (names.bagValue || BAG_VALUE) })] // EXPLICIT ANY value
        }),
        new asn1js.Set({
          optional: true,
          value: [
            new asn1js.Repeated({
              name: (names.bagAttributes || BAG_ATTRIBUTES),
              value: Attribute.schema()
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
      SafeBag.schema({
        names: {
          bagId: BAG_ID,
          bagValue: BAG_VALUE,
          bagAttributes: BAG_ATTRIBUTES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.bagId = asn1.result.bagId.valueBlock.toString();

    const bagType = SafeBagValueFactory.find(this.bagId);
    if (!bagType) {
      throw new Error(`Invalid BAG_ID for SafeBag: ${this.bagId}`);
    }
    this.bagValue = new bagType({ schema: asn1.result.bagValue }) as unknown as T;

    if (BAG_ATTRIBUTES in asn1.result) {
      this.bagAttributes = Array.from(asn1.result.bagAttributes, element => new Attribute({ schema: element }));
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    const outputArray = [
      new asn1js.ObjectIdentifier({ value: this.bagId }),
      new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [this.bagValue.toSchema()]
      })
    ];

    if (this.bagAttributes) {
      outputArray.push(new asn1js.Set({
        value: Array.from(this.bagAttributes, o => o.toSchema())
      }));
    }

    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): SafeBagJson {
    const output: SafeBagJson = {
      bagId: this.bagId,
      bagValue: this.bagValue.toJSON()
    };

    if (this.bagAttributes) {
      output.bagAttributes = Array.from(this.bagAttributes, o => o.toJSON());
    }

    return output;
  }

}

import { type BagType, SafeBagValueFactory, BagTypeJson } from "./SafeBagValueFactory";
import { EMPTY_STRING } from "./constants";

