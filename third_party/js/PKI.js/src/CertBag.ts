import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { Certificate } from "./Certificate";
import { AttributeCertificateV2 } from "./AttributeCertificateV2";
import * as Schema from "./Schema";
import { id_CertBag_AttributeCertificate, id_CertBag_SDSICertificate, id_CertBag_X509Certificate } from "./ObjectIdentifiers";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { EMPTY_STRING } from "./constants";

const CERT_ID = "certId";
const CERT_VALUE = "certValue";
const PARSED_VALUE = "parsedValue";
const CLEAR_PROPS = [
  CERT_ID,
  CERT_VALUE
];

export interface ICertBag {
  certId: string;
  certValue: asn1js.OctetString | PkiObject;
  parsedValue: any;
}

export type CertBagParameters = PkiObjectParameters & Partial<ICertBag>;

export interface CertBagJson {
  certId: string;
  certValue: any;
}

/**
 * Represents the CertBag structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class CertBag extends PkiObject implements ICertBag {

  public static override CLASS_NAME = "CertBag";

  public certId!: string;
  public certValue: PkiObject | asn1js.OctetString;
  public parsedValue: any;

  /**
   * Initializes a new instance of the {@link CertBag} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CertBagParameters = {}) {
    super();

    this.certId = pvutils.getParametersValue(parameters, CERT_ID, CertBag.defaultValues(CERT_ID));
    this.certValue = pvutils.getParametersValue(parameters, CERT_VALUE, CertBag.defaultValues(CERT_VALUE));
    if (PARSED_VALUE in parameters) {
      this.parsedValue = pvutils.getParametersValue(parameters, PARSED_VALUE, CertBag.defaultValues(PARSED_VALUE));
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
  public static override defaultValues(memberName: typeof CERT_ID): string;
  public static override defaultValues(memberName: typeof CERT_VALUE): any;
  public static override defaultValues(memberName: typeof PARSED_VALUE): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CERT_ID:
        return EMPTY_STRING;
      case CERT_VALUE:
        return (new asn1js.Any());
      case PARSED_VALUE:
        return {};
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
      case CERT_ID:
        return (memberValue === EMPTY_STRING);
      case CERT_VALUE:
        return (memberValue instanceof asn1js.Any);
      case PARSED_VALUE:
        return ((memberValue instanceof Object) && (Object.keys(memberValue).length === 0));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * CertBag ::= SEQUENCE {
   *    certId    BAG-TYPE.&id   ({CertTypes}),
   *    certValue [0] EXPLICIT BAG-TYPE.&Type ({CertTypes}{@certId})
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    id?: string;
    value?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.id || "id") }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [new asn1js.Any({ name: (names.value || "value") })] // EXPLICIT ANY value
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
      CertBag.schema({
        names: {
          id: CERT_ID,
          value: CERT_VALUE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    this.certId = asn1.result.certId.valueBlock.toString();
    this.certValue = asn1.result.certValue as asn1js.OctetString;

    const certValueHex = this.certValue.valueBlock.valueHexView;
    switch (this.certId) {
      case id_CertBag_X509Certificate: // x509Certificate
        {
          try {
            this.parsedValue = Certificate.fromBER(certValueHex);
          }
          catch (ex) // In some realizations the same OID used for attribute certificates
          {
            AttributeCertificateV2.fromBER(certValueHex);
          }
        }
        break;
      case id_CertBag_AttributeCertificate: // attributeCertificate - (!!!) THIS OID IS SUBJECT FOR CHANGE IN FUTURE (!!!)
        {
          this.parsedValue = AttributeCertificateV2.fromBER(certValueHex);
        }
        break;
      case id_CertBag_SDSICertificate: // sdsiCertificate
      default:
        throw new Error(`Incorrect CERT_ID value in CertBag: ${this.certId}`);
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    if (PARSED_VALUE in this) {
      if ("acinfo" in this.parsedValue) {// attributeCertificate
        this.certId = id_CertBag_AttributeCertificate;
      } else {// x509Certificate
        this.certId = id_CertBag_X509Certificate;
      }

      this.certValue = new asn1js.OctetString({ valueHex: this.parsedValue.toSchema().toBER(false) });
    }

    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.certId }),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [(("toSchema" in this.certValue) ? this.certValue.toSchema() : this.certValue)]
        })
      ]
    }));
    //#endregion
  }

  public toJSON(): CertBagJson {
    return {
      certId: this.certId,
      certValue: this.certValue.toJSON()
    };
  }

}
