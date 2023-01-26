import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { CertificateRevocationList, CertificateRevocationListJson } from "./CertificateRevocationList";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { OtherRevocationInfoFormat, OtherRevocationInfoFormatJson } from "./OtherRevocationInfoFormat";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const CRLS = "crls";
const OTHER_REVOCATION_INFOS = "otherRevocationInfos";
const CLEAR_PROPS = [
  CRLS
];

export interface IRevocationInfoChoices {
  crls: CertificateRevocationList[];
  otherRevocationInfos: OtherRevocationInfoFormat[];
}

export interface RevocationInfoChoicesJson {
  crls: CertificateRevocationListJson[];
  otherRevocationInfos: OtherRevocationInfoFormatJson[];
}

export type RevocationInfoChoicesParameters = PkiObjectParameters & Partial<IRevocationInfoChoices>;

export type RevocationInfoChoicesSchema = Schema.SchemaParameters<{
  crls?: string;
}>;

/**
 * Represents the RevocationInfoChoices structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class RevocationInfoChoices extends PkiObject implements IRevocationInfoChoices {

  public static override CLASS_NAME = "RevocationInfoChoices";

  public crls!: CertificateRevocationList[];
  public otherRevocationInfos!: OtherRevocationInfoFormat[];

  /**
   * Initializes a new instance of the {@link RevocationInfoChoices} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RevocationInfoChoicesParameters = {}) {
    super();

    this.crls = pvutils.getParametersValue(parameters, CRLS, RevocationInfoChoices.defaultValues(CRLS));
    this.otherRevocationInfos = pvutils.getParametersValue(parameters, OTHER_REVOCATION_INFOS, RevocationInfoChoices.defaultValues(OTHER_REVOCATION_INFOS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof CRLS): CertificateRevocationList[];
  public static override defaultValues(memberName: typeof OTHER_REVOCATION_INFOS): OtherRevocationInfoFormat[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CRLS:
        return [];
      case OTHER_REVOCATION_INFOS:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * RevocationInfoChoices ::= SET OF RevocationInfoChoice
   *
   * RevocationInfoChoice ::= CHOICE {
   *    crl CertificateList,
   *    other [1] IMPLICIT OtherRevocationInfoFormat }
   *```
   */
  public static override schema(parameters: RevocationInfoChoicesSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Set({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Repeated({
          name: (names.crls || EMPTY_STRING),
          value: new asn1js.Choice({
            value: [
              CertificateRevocationList.schema(),
              new asn1js.Constructed({
                idBlock: {
                  tagClass: 3, // CONTEXT-SPECIFIC
                  tagNumber: 1 // [1]
                },
                value: [
                  new asn1js.ObjectIdentifier(),
                  new asn1js.Any()
                ]
              })
            ]
          })
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
      RevocationInfoChoices.schema({
        names: {
          crls: CRLS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (asn1.result.crls) {
      for (const element of asn1.result.crls) {
        if (element.idBlock.tagClass === 1)
          this.crls.push(new CertificateRevocationList({ schema: element }));
        else
          this.otherRevocationInfos.push(new OtherRevocationInfoFormat({ schema: element }));
      }
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output set
    const outputArray = [];

    outputArray.push(...Array.from(this.crls, o => o.toSchema()));

    outputArray.push(...Array.from(this.otherRevocationInfos, element => {
      const schema = element.toSchema();

      schema.idBlock.tagClass = 3;
      schema.idBlock.tagNumber = 1;

      return schema;
    }));
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Set({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): RevocationInfoChoicesJson {
    return {
      crls: Array.from(this.crls, o => o.toJSON()),
      otherRevocationInfos: Array.from(this.otherRevocationInfos, o => o.toJSON())
    };
  }

}
