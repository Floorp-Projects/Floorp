import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";

const SAFE_BUGS = "safeBags";

export interface ISafeContents {
  safeBags: SafeBag[];
}

export type SafeContentsParameters = PkiObjectParameters & Partial<ISafeContents>;

export interface SafeContentsJson {
  safeBags: SafeBagJson[];
}

/**
 * Represents the SafeContents structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class SafeContents extends PkiObject implements ISafeContents {

  public static override CLASS_NAME = "SafeContents";

  public safeBags!: SafeBag[];

  /**
   * Initializes a new instance of the {@link SafeContents} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: SafeContentsParameters = {}) {
    super();

    this.safeBags = pvutils.getParametersValue(parameters, SAFE_BUGS, SafeContents.defaultValues(SAFE_BUGS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof SAFE_BUGS): SafeBag[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case SAFE_BUGS:
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
      case SAFE_BUGS:
        return (memberValue.length === 0);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * SafeContents ::= SEQUENCE OF SafeBag
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    safeBags?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.safeBags || EMPTY_STRING),
          value: SafeBag.schema()
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, [
      SAFE_BUGS
    ]);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      SafeContents.schema({
        names: {
          safeBags: SAFE_BUGS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.safeBags = Array.from(asn1.result.safeBags, element => new SafeBag({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.safeBags, o => o.toSchema())
    }));
    //#endregion
  }

  public toJSON(): SafeContentsJson {
    return {
      safeBags: Array.from(this.safeBags, o => o.toJSON())
    };
  }

}

import { SafeBag, SafeBagJson } from "./SafeBag";import { EMPTY_STRING } from "./constants";

