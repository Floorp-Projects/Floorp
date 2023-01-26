import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RelativeDistinguishedNames, RelativeDistinguishedNamesJson, RelativeDistinguishedNamesSchema } from "./RelativeDistinguishedNames";
import * as Schema from "./Schema";

const ISSUER = "issuer";
const SERIAL_NUMBER = "serialNumber";
const CLEAR_PROPS = [
  ISSUER,
  SERIAL_NUMBER,
];

export interface IIssuerAndSerialNumber {
  /**
   * Certificate issuer name
   */
  issuer: RelativeDistinguishedNames;
  /**
   * Certificate serial number
   */
  serialNumber: asn1js.Integer;
}

export interface IssuerAndSerialNumberJson {
  issuer: RelativeDistinguishedNamesJson;
  serialNumber: asn1js.IntegerJson;
}

export type IssuerAndSerialNumberParameters = PkiObjectParameters & Partial<IIssuerAndSerialNumber>;

export type IssuerAndSerialNumberSchema = Schema.SchemaParameters<{
  issuer?: RelativeDistinguishedNamesSchema;
  serialNumber?: string;
}>;

/**
 * Represents the IssuerAndSerialNumber structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class IssuerAndSerialNumber extends PkiObject implements IIssuerAndSerialNumber {

  public static override CLASS_NAME = "IssuerAndSerialNumber";

  public issuer!: RelativeDistinguishedNames;
  public serialNumber!: asn1js.Integer;

  /**
   * Initializes a new instance of the {@link IssuerAndSerialNumber} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: IssuerAndSerialNumberParameters = {}) {
    super();

    this.issuer = pvutils.getParametersValue(parameters, ISSUER, IssuerAndSerialNumber.defaultValues(ISSUER));
    this.serialNumber = pvutils.getParametersValue(parameters, SERIAL_NUMBER, IssuerAndSerialNumber.defaultValues(SERIAL_NUMBER));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ISSUER): RelativeDistinguishedNames;
  public static override defaultValues(memberName: typeof SERIAL_NUMBER): asn1js.Integer;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ISSUER:
        return new RelativeDistinguishedNames();
      case SERIAL_NUMBER:
        return new asn1js.Integer();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * IssuerAndSerialNumber ::= SEQUENCE {
   *    issuer Name,
   *    serialNumber CertificateSerialNumber }
   *
   * CertificateSerialNumber ::= INTEGER
   *```
   */
  public static override schema(parameters: IssuerAndSerialNumberSchema = {}): Schema.SchemaType {
    /**
     * @type {Object}
     * @property {string} [blockName]
     * @property {string} [issuer]
     * @property {string} [serialNumber]
     */
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        RelativeDistinguishedNames.schema(names.issuer || {}),
        new asn1js.Integer({ name: (names.serialNumber || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      IssuerAndSerialNumber.schema({
        names: {
          issuer: {
            names: {
              blockName: ISSUER
            }
          },
          serialNumber: SERIAL_NUMBER
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.issuer = new RelativeDistinguishedNames({ schema: asn1.result.issuer });
    this.serialNumber = asn1.result.serialNumber;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        this.issuer.toSchema(),
        this.serialNumber
      ]
    }));
  }

  public toJSON(): IssuerAndSerialNumberJson {
    return {
      issuer: this.issuer.toJSON(),
      serialNumber: this.serialNumber.toJSON(),
    };
  }

}
