import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const OTHER_REV_INFO_FORMAT = "otherRevInfoFormat";
const OTHER_REV_INFO = "otherRevInfo";
const CLEAR_PROPS = [
  OTHER_REV_INFO_FORMAT,
  OTHER_REV_INFO
];

export interface IOtherRevocationInfoFormat {
  otherRevInfoFormat: string;
  otherRevInfo: any;
}

export interface OtherRevocationInfoFormatJson {
  otherRevInfoFormat: string;
  otherRevInfo?: any;
}

export type OtherRevocationInfoFormatParameters = PkiObjectParameters & Partial<IOtherRevocationInfoFormat>;

/**
 * Represents the OtherRevocationInfoFormat structure described in [RFC5652](https://datatracker.ietf.org/doc/html/rfc5652)
 */
export class OtherRevocationInfoFormat extends PkiObject implements IOtherRevocationInfoFormat {

  public static override CLASS_NAME = "OtherRevocationInfoFormat";

  public otherRevInfoFormat!: string;
  public otherRevInfo: any;

  /**
   * Initializes a new instance of the {@link OtherRevocationInfoFormat} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: OtherRevocationInfoFormatParameters = {}) {
    super();

    this.otherRevInfoFormat = pvutils.getParametersValue(parameters, OTHER_REV_INFO_FORMAT, OtherRevocationInfoFormat.defaultValues(OTHER_REV_INFO_FORMAT));
    this.otherRevInfo = pvutils.getParametersValue(parameters, OTHER_REV_INFO, OtherRevocationInfoFormat.defaultValues(OTHER_REV_INFO));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  static override defaultValues(memberName: typeof OTHER_REV_INFO_FORMAT): string;
  static override defaultValues(memberName: typeof OTHER_REV_INFO): any;
  static override defaultValues(memberName: string): any {
    switch (memberName) {
      case OTHER_REV_INFO_FORMAT:
        return EMPTY_STRING;
      case OTHER_REV_INFO:
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
   *    otherRevInfoFormat OBJECT IDENTIFIER,
   *    otherRevInfo ANY DEFINED BY otherCertFormat }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    otherRevInfoFormat?: string;
    otherRevInfo?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.otherRevInfoFormat || OTHER_REV_INFO_FORMAT) }),
        new asn1js.Any({ name: (names.otherRevInfo || OTHER_REV_INFO) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      OtherRevocationInfoFormat.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.otherRevInfoFormat = asn1.result.otherRevInfoFormat.valueBlock.toString();
    this.otherRevInfo = asn1.result.otherRevInfo;
  }

  public toSchema(): asn1js.Sequence {
    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: [
        new asn1js.ObjectIdentifier({ value: this.otherRevInfoFormat }),
        this.otherRevInfo
      ]
    }));
  }

  public toJSON(): OtherRevocationInfoFormatJson {
    const res: OtherRevocationInfoFormatJson = {
      otherRevInfoFormat: this.otherRevInfoFormat
    };

    if (!(this.otherRevInfo instanceof asn1js.Any)) {
      res.otherRevInfo = this.otherRevInfo.toJSON();
    }

    return res;
  }

}
