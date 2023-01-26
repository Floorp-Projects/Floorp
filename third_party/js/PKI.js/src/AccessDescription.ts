import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { GeneralName, GeneralNameJson, GeneralNameSchema } from "./GeneralName";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ACCESS_METHOD = "accessMethod";
const ACCESS_LOCATION = "accessLocation";
const CLEAR_PROPS = [
  ACCESS_METHOD,
  ACCESS_LOCATION,
];

export interface IAccessDescription {
  /**
   * The type and format of the information are specified by the accessMethod field. This profile defines two accessMethod OIDs: id-ad-caIssuers and id-ad-ocsp
   */
  accessMethod: string;
  /**
   * The accessLocation field specifies the location of the information
   */
  accessLocation: GeneralName;
}

export type AccessDescriptionParameters = PkiObjectParameters & Partial<IAccessDescription>;

/**
 * JSON representation of {@link AccessDescription}
 */
export interface AccessDescriptionJson {
  accessMethod: string;
  accessLocation: GeneralNameJson;
}

/**
 * Represents the AccessDescription structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 *
 * The authority information access extension indicates how to access
 * information and services for the issuer of the certificate in which
 * the extension appears. Information and services may include on-line
 * validation services and CA policy data. This extension may be included in
 * end entity or CA certificates. Conforming CAs MUST mark this
 * extension as non-critical.
 */
export class AccessDescription extends PkiObject implements IAccessDescription {

  public static override CLASS_NAME = "AccessDescription";

  public accessMethod!: string;
  public accessLocation!: GeneralName;

  /**
   * Initializes a new instance of the {@link AccessDescription} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AccessDescriptionParameters = {}) {
    super();

    this.accessMethod = pvutils.getParametersValue(parameters, ACCESS_METHOD, AccessDescription.defaultValues(ACCESS_METHOD));
    this.accessLocation = pvutils.getParametersValue(parameters, ACCESS_LOCATION, AccessDescription.defaultValues(ACCESS_LOCATION));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ACCESS_METHOD): string;
  public static override defaultValues(memberName: typeof ACCESS_LOCATION): GeneralName;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ACCESS_METHOD:
        return EMPTY_STRING;
      case ACCESS_LOCATION:
        return new GeneralName();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AccessDescription ::= SEQUENCE {
   *    accessMethod          OBJECT IDENTIFIER,
   *    accessLocation        GeneralName  }
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{ accessMethod?: string; accessLocation?: GeneralNameSchema; }> = {}) {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.accessMethod || EMPTY_STRING) }),
        GeneralName.schema(names.accessLocation || {})
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      AccessDescription.schema({
        names: {
          accessMethod: ACCESS_METHOD,
          accessLocation: {
            names: {
              blockName: ACCESS_LOCATION
            }
          }
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.accessMethod = asn1.result.accessMethod.valueBlock.toString();
    this.accessLocation = new GeneralName({ schema: asn1.result.accessLocation });
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.accessMethod }),
        this.accessLocation.toSchema()
      ]
    }));
    //#endregion
  }

  public toJSON(): AccessDescriptionJson {
    return {
      accessMethod: this.accessMethod,
      accessLocation: this.accessLocation.toJSON()
    };
  }

}
