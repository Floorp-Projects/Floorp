import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const TEMPLATE_ID = "templateID";
const TEMPLATE_MAJOR_VERSION = "templateMajorVersion";
const TEMPLATE_MINOR_VERSION = "templateMinorVersion";
const CLEAR_PROPS = [
  TEMPLATE_ID,
  TEMPLATE_MAJOR_VERSION,
  TEMPLATE_MINOR_VERSION
];

export interface ICertificateTemplate {
  templateID: string;
  templateMajorVersion?: number;
  templateMinorVersion?: number;
}

export interface CertificateTemplateJson {
  templateID: string;
  templateMajorVersion?: number;
  templateMinorVersion?: number;
}

export type CertificateTemplateParameters = PkiObjectParameters & Partial<ICertificateTemplate>;

/**
 * Class from "[MS-WCCE]: Windows Client Certificate Enrollment Protocol"
 */
export class CertificateTemplate extends PkiObject implements ICertificateTemplate {

  public templateID!: string;
  public templateMajorVersion?: number;
  public templateMinorVersion?: number;

  /**
   * Initializes a new instance of the {@link CertificateTemplate} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: CertificateTemplateParameters = {}) {
    super();

    this.templateID = pvutils.getParametersValue(parameters, TEMPLATE_ID, CertificateTemplate.defaultValues(TEMPLATE_ID));
    if (TEMPLATE_MAJOR_VERSION in parameters) {
      this.templateMajorVersion = pvutils.getParametersValue(parameters, TEMPLATE_MAJOR_VERSION, CertificateTemplate.defaultValues(TEMPLATE_MAJOR_VERSION));
    }
    if (TEMPLATE_MINOR_VERSION in parameters) {
      this.templateMinorVersion = pvutils.getParametersValue(parameters, TEMPLATE_MINOR_VERSION, CertificateTemplate.defaultValues(TEMPLATE_MINOR_VERSION));
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
  public static override defaultValues(memberName: typeof TEMPLATE_MINOR_VERSION): number;
  public static override defaultValues(memberName: typeof TEMPLATE_MAJOR_VERSION): number;
  public static override defaultValues(memberName: typeof TEMPLATE_ID): string;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case TEMPLATE_ID:
        return EMPTY_STRING;
      case TEMPLATE_MAJOR_VERSION:
      case TEMPLATE_MINOR_VERSION:
        return 0;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * CertificateTemplateOID ::= SEQUENCE {
   *    templateID              OBJECT IDENTIFIER,
   *    templateMajorVersion    INTEGER (0..4294967295) OPTIONAL,
   *    templateMinorVersion    INTEGER (0..4294967295) OPTIONAL
   * }
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{
    templateID?: string,
    templateMajorVersion?: string,
    templateMinorVersion?: string,
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.templateID || EMPTY_STRING) }),
        new asn1js.Integer({
          name: (names.templateMajorVersion || EMPTY_STRING),
          optional: true
        }),
        new asn1js.Integer({
          name: (names.templateMinorVersion || EMPTY_STRING),
          optional: true
        }),
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      CertificateTemplate.schema({
        names: {
          templateID: TEMPLATE_ID,
          templateMajorVersion: TEMPLATE_MAJOR_VERSION,
          templateMinorVersion: TEMPLATE_MINOR_VERSION
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.templateID = asn1.result.templateID.valueBlock.toString();
    if (TEMPLATE_MAJOR_VERSION in asn1.result) {
      this.templateMajorVersion = asn1.result.templateMajorVersion.valueBlock.valueDec;
    }
    if (TEMPLATE_MINOR_VERSION in asn1.result) {
      this.templateMinorVersion = asn1.result.templateMinorVersion.valueBlock.valueDec;
    }
  }

  public toSchema(): asn1js.Sequence {
    // Create array for output sequence
    const outputArray = [];
    outputArray.push(new asn1js.ObjectIdentifier({ value: this.templateID }));
    if (TEMPLATE_MAJOR_VERSION in this) {
      outputArray.push(new asn1js.Integer({ value: this.templateMajorVersion }));
    }
    if (TEMPLATE_MINOR_VERSION in this) {
      outputArray.push(new asn1js.Integer({ value: this.templateMinorVersion }));
    }

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toJSON(): CertificateTemplateJson {
    const res: CertificateTemplateJson = {
      templateID: this.templateID
    };

    if (TEMPLATE_MAJOR_VERSION in this)
      res.templateMajorVersion = this.templateMajorVersion;

    if (TEMPLATE_MINOR_VERSION in this)
      res.templateMinorVersion = this.templateMinorVersion;

    return res;
  }

}
