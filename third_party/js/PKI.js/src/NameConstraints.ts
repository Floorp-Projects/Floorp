import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { GeneralSubtree, GeneralSubtreeJson } from "./GeneralSubtree";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const PERMITTED_SUBTREES = "permittedSubtrees";
const EXCLUDED_SUBTREES = "excludedSubtrees";
const CLEAR_PROPS = [
  PERMITTED_SUBTREES,
  EXCLUDED_SUBTREES
];

export interface INameConstraints {
  permittedSubtrees?: GeneralSubtree[];
  excludedSubtrees?: GeneralSubtree[];
}

export interface NameConstraintsJson {
  permittedSubtrees?: GeneralSubtreeJson[];
  excludedSubtrees?: GeneralSubtreeJson[];
}

export type NameConstraintsParameters = PkiObjectParameters & Partial<INameConstraints>;

/**
 * Represents the NameConstraints structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class NameConstraints extends PkiObject implements INameConstraints {

  public static override CLASS_NAME = "NameConstraints";

  public permittedSubtrees?: GeneralSubtree[];
  public excludedSubtrees?: GeneralSubtree[];

  /**
   * Initializes a new instance of the {@link NameConstraints} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: NameConstraintsParameters = {}) {
    super();

    if (PERMITTED_SUBTREES in parameters) {
      this.permittedSubtrees = pvutils.getParametersValue(parameters, PERMITTED_SUBTREES, NameConstraints.defaultValues(PERMITTED_SUBTREES));
    }
    if (EXCLUDED_SUBTREES in parameters) {
      this.excludedSubtrees = pvutils.getParametersValue(parameters, EXCLUDED_SUBTREES, NameConstraints.defaultValues(EXCLUDED_SUBTREES));
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
  public static override defaultValues(memberName: typeof PERMITTED_SUBTREES): GeneralSubtree[];
  public static override defaultValues(memberName: typeof EXCLUDED_SUBTREES): GeneralSubtree[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case PERMITTED_SUBTREES:
      case EXCLUDED_SUBTREES:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * NameConstraints ::= SEQUENCE {
   *    permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
   *    excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    permittedSubtrees?: string;
    excludedSubtrees?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            new asn1js.Repeated({
              name: (names.permittedSubtrees || EMPTY_STRING),
              value: GeneralSubtree.schema()
            })
          ]
        }),
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [
            new asn1js.Repeated({
              name: (names.excludedSubtrees || EMPTY_STRING),
              value: GeneralSubtree.schema()
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
      NameConstraints.schema({
        names: {
          permittedSubtrees: PERMITTED_SUBTREES,
          excludedSubtrees: EXCLUDED_SUBTREES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (PERMITTED_SUBTREES in asn1.result)
      this.permittedSubtrees = Array.from(asn1.result.permittedSubtrees, element => new GeneralSubtree({ schema: element }));
    if (EXCLUDED_SUBTREES in asn1.result)
      this.excludedSubtrees = Array.from(asn1.result.excludedSubtrees, element => new GeneralSubtree({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (this.permittedSubtrees) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: Array.from(this.permittedSubtrees, o => o.toSchema())
      }));
    }

    if (this.excludedSubtrees) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        value: Array.from(this.excludedSubtrees, o => o.toSchema())
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): NameConstraintsJson {
    const object: NameConstraintsJson = {};

    if (this.permittedSubtrees) {
      object.permittedSubtrees = Array.from(this.permittedSubtrees, o => o.toJSON());
    }

    if (this.excludedSubtrees) {
      object.excludedSubtrees = Array.from(this.excludedSubtrees, o => o.toJSON());
    }

    return object;
  }

}
