import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AccessDescription, AccessDescriptionJson } from "./AccessDescription";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ACCESS_DESCRIPTIONS = "accessDescriptions";

export interface IInfoAccess {
  accessDescriptions: AccessDescription[];
}

export interface InfoAccessJson {
  accessDescriptions: AccessDescriptionJson[];
}

export type InfoAccessParameters = PkiObjectParameters & Partial<IInfoAccess>;

/**
 * Represents the InfoAccess structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class InfoAccess extends PkiObject implements IInfoAccess {

  public static override CLASS_NAME = "InfoAccess";

  public accessDescriptions!: AccessDescription[];

  /**
   * Initializes a new instance of the {@link InfoAccess} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: InfoAccessParameters = {}) {
    super();

    this.accessDescriptions = pvutils.getParametersValue(parameters, ACCESS_DESCRIPTIONS, InfoAccess.defaultValues(ACCESS_DESCRIPTIONS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ACCESS_DESCRIPTIONS): AccessDescription[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ACCESS_DESCRIPTIONS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AuthorityInfoAccessSyntax  ::=
   * SEQUENCE SIZE (1..MAX) OF AccessDescription
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    accessDescriptions?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.accessDescriptions || EMPTY_STRING),
          value: AccessDescription.schema()
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, [
      ACCESS_DESCRIPTIONS
    ]);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      InfoAccess.schema({
        names: {
          accessDescriptions: ACCESS_DESCRIPTIONS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.accessDescriptions = Array.from(asn1.result.accessDescriptions, element => new AccessDescription({ schema: element }));
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: Array.from(this.accessDescriptions, o => o.toSchema())
    }));
  }

  public toJSON(): InfoAccessJson {
    return {
      accessDescriptions: Array.from(this.accessDescriptions, o => o.toJSON())
    };
  }

}

