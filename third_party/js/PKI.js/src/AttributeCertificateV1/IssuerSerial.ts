import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "../constants";
import { AsnError } from "../errors";
import { GeneralNames, GeneralNamesJson, GeneralNamesSchema } from "../GeneralNames";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import * as Schema from "../Schema";

const ISSUER = "issuer";
const SERIAL_NUMBER = "serialNumber";
const ISSUER_UID = "issuerUID";
const CLEAR_PROPS = [
  ISSUER,
  SERIAL_NUMBER,
  ISSUER_UID,
];

export interface IIssuerSerial {
  /**
   * Issuer name
   */
  issuer: GeneralNames;
  /**
   * Serial number
   */
  serialNumber: asn1js.Integer;
  /**
   * Issuer unique identifier
   */
  issuerUID?: asn1js.BitString;
}

export type IssuerSerialParameters = PkiObjectParameters & Partial<IIssuerSerial>;

export interface IssuerSerialJson {
  issuer: GeneralNamesJson;
  serialNumber: asn1js.IntegerJson;
  issuerUID?: asn1js.BitStringJson;
}

/**
 * Represents the IssuerSerial structure described in [RFC5755](https://datatracker.ietf.org/doc/html/rfc5755)
 */
export class IssuerSerial extends PkiObject implements IIssuerSerial {

  public static override CLASS_NAME = "IssuerSerial";

  public issuer!: GeneralNames;
  public serialNumber!: asn1js.Integer;
  public issuerUID?: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link IssuerSerial} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: IssuerSerialParameters = {}) {
    super();

    this.issuer = pvutils.getParametersValue(parameters, ISSUER, IssuerSerial.defaultValues(ISSUER));
    this.serialNumber = pvutils.getParametersValue(parameters, SERIAL_NUMBER, IssuerSerial.defaultValues(SERIAL_NUMBER));
    if (ISSUER_UID in parameters) {
      this.issuerUID = pvutils.getParametersValue(parameters, ISSUER_UID, IssuerSerial.defaultValues(ISSUER_UID));
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
  public static override defaultValues(memberName: typeof ISSUER): GeneralNames;
  public static override defaultValues(memberName: typeof SERIAL_NUMBER): asn1js.Integer;
  public static override defaultValues(memberName: typeof ISSUER_UID): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ISSUER:
        return new GeneralNames();
      case SERIAL_NUMBER:
        return new asn1js.Integer();
      case ISSUER_UID:
        return new asn1js.BitString();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * IssuerSerial ::= SEQUENCE {
   *     issuer         GeneralNames,
   *     serial         CertificateSerialNumber,
   *     issuerUID      UniqueIdentifier OPTIONAL
   * }
   *
   * CertificateSerialNumber ::= INTEGER
   * UniqueIdentifier ::= BIT STRING
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    issuer?: GeneralNamesSchema;
    serialNumber?: string;
    issuerUID?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        GeneralNames.schema(names.issuer || {}),
        new asn1js.Integer({ name: (names.serialNumber || EMPTY_STRING) }),
        new asn1js.BitString({
          optional: true,
          name: (names.issuerUID || EMPTY_STRING)
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);
    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      IssuerSerial.schema({
        names: {
          issuer: {
            names: {
              blockName: ISSUER
            }
          },
          serialNumber: SERIAL_NUMBER,
          issuerUID: ISSUER_UID
        }
      })
    );

    AsnError.assertSchema(asn1, this.className);
    //#endregion
    //#region Get internal properties from parsed schema
    this.issuer = new GeneralNames({ schema: asn1.result.issuer });
    this.serialNumber = asn1.result.serialNumber;

    if (ISSUER_UID in asn1.result)
      this.issuerUID = asn1.result.issuerUID;
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    const result = new asn1js.Sequence({
      value: [
        this.issuer.toSchema(),
        this.serialNumber
      ]
    });

    if (this.issuerUID) {
      result.valueBlock.value.push(this.issuerUID);
    }

    return result;
  }

  public toJSON(): IssuerSerialJson {
    const result = {
      issuer: this.issuer.toJSON(),
      serialNumber: this.serialNumber.toJSON()
    } as IssuerSerialJson;

    if (this.issuerUID) {
      result.issuerUID = this.issuerUID.toJSON();
    }

    return result;
  }

}
