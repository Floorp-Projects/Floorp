import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import * as Schema from "./Schema";
import { GeneralName, GeneralNameJson } from "./GeneralName";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const NAMES = "names";
const GENERAL_NAMES = "generalNames";

export interface IGeneralNames {
  names: GeneralName[];
}

export type GeneralNamesParameters = PkiObjectParameters & Partial<IGeneralNames>;

export type GeneralNamesSchema = Schema.SchemaParameters<{
  generalNames?: string;
}>;

export interface GeneralNamesJson {
  names: GeneralNameJson[];
}

/**
 * Represents the GeneralNames structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class GeneralNames extends PkiObject implements IGeneralNames {

  public static override CLASS_NAME = "GeneralNames";

  /**
   * Array of "general names"
   */
  public names!: GeneralName[];

  /**
   * Initializes a new instance of the {@link GeneralNames} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: GeneralNamesParameters = {}) {
    super();

    this.names = pvutils.getParametersValue(parameters, NAMES, GeneralNames.defaultValues(NAMES));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof NAMES): GeneralName[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case "names":
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
   * ```
   *
   * @param parameters Input parameters for the schema
   * @param optional Flag would be element optional or not
   * @returns ASN.1 schema object
   */
  public static override schema(parameters: GeneralNamesSchema = {}, optional = false): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, NAMES, {});

    return (new asn1js.Sequence({
      optional,
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.generalNames || EMPTY_STRING),
          value: GeneralName.schema()
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    //#region Clear input data first
    pvutils.clearProps(schema, [
      NAMES,
      GENERAL_NAMES
    ]);
    //#endregion

    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      GeneralNames.schema({
        names: {
          blockName: NAMES,
          generalNames: GENERAL_NAMES
        }
      })
    );

    AsnError.assertSchema(asn1, this.className);
    //#endregion

    this.names = Array.from(asn1.result.generalNames, element => new GeneralName({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    return (new asn1js.Sequence({
      value: Array.from(this.names, o => o.toSchema())
    }));
  }

  public toJSON(): GeneralNamesJson {
    return {
      names: Array.from(this.names, o => o.toJSON())
    };
  }

}
