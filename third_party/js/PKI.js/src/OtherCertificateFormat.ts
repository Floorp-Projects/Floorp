import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const OTHER_CERT_FORMAT = "otherCertFormat";
const OTHER_CERT = "otherCert";
const CLEAR_PROPS = [
  OTHER_CERT_FORMAT,
  OTHER_CERT
];

export interface IOtherCertificateFormat {
  otherCertFormat: string;
  otherCert: any;
}

export interface OtherCertificateFormatJson {
  otherCertFormat: string;
  otherCert?: any;
}

export type OtherCertificateFormatParameters = PkiObjectParameters & Partial<IOtherCertificateFormat>;

/**
 * Represents the OtherCertificateFormat structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class OtherCertificateFormat extends PkiObject implements IOtherCertificateFormat {

  public otherCertFormat!: string;
  public otherCert: any;

  /**
   * Initializes a new instance of the {@link OtherCertificateFormat} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OtherCertificateFormatParameters = {}) {
    super();

    this.otherCertFormat = pvutils.getParametersValue(parameters, OTHER_CERT_FORMAT, OtherCertificateFormat.defaultValues(OTHER_CERT_FORMAT));
    this.otherCert = pvutils.getParametersValue(parameters, OTHER_CERT, OtherCertificateFormat.defaultValues(OTHER_CERT));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof OTHER_CERT_FORMAT): string;
  public static override defaultValues(memberName: typeof OTHER_CERT): asn1js.Any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case OTHER_CERT_FORMAT:
        return EMPTY_STRING;
      case OTHER_CERT:
        return new asn1js.Any();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * OtherCertificateFormat ::= SEQUENCE {
   *    otherCertFormat OBJECT IDENTIFIER,
   *    otherCert ANY DEFINED BY otherCertFormat }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    otherCertFormat?: string;
    otherCert?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.otherCertFormat || OTHER_CERT_FORMAT) }),
        new asn1js.Any({ name: (names.otherCert || OTHER_CERT) })
      ]
    }));
  }

  public fromSchema(schema: any): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      OtherCertificateFormat.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.otherCertFormat = asn1.result.otherCertFormat.valueBlock.toString();
    this.otherCert = asn1.result.otherCert;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.otherCertFormat }),
        this.otherCert
      ]
    }));
  }

  public toJSON(): OtherCertificateFormatJson {
    const res: OtherCertificateFormatJson = {
      otherCertFormat: this.otherCertFormat
    };

    if (!(this.otherCert instanceof asn1js.Any)) {
      res.otherCert = this.otherCert.toJSON();
    }

    return res;
  }

}
