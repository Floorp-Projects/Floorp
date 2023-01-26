import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { Time, TimeJson } from "./Time";
import { Extensions, ExtensionsJson } from "./Extensions";
import * as Schema from "./Schema";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { AsnError } from "./errors";
import { EMPTY_STRING } from "./constants";

const USER_CERTIFICATE = "userCertificate";
const REVOCATION_DATE = "revocationDate";
const CRL_ENTRY_EXTENSIONS = "crlEntryExtensions";
const CLEAR_PROPS = [
  USER_CERTIFICATE,
  REVOCATION_DATE,
  CRL_ENTRY_EXTENSIONS
];

export interface IRevokedCertificate {
  userCertificate: asn1js.Integer;
  revocationDate: Time;
  crlEntryExtensions?: Extensions;
}

export type RevokedCertificateParameters = PkiObjectParameters & Partial<IRevokedCertificate>;

export interface RevokedCertificateJson {
  userCertificate: asn1js.IntegerJson;
  revocationDate: TimeJson;
  crlEntryExtensions?: ExtensionsJson;
}

/**
 * Represents the RevokedCertificate structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class RevokedCertificate extends PkiObject implements IRevokedCertificate {

  public static override CLASS_NAME = "RevokedCertificate";

  public userCertificate!: asn1js.Integer;
  public revocationDate!: Time;
  public crlEntryExtensions?: Extensions;

  /**
   * Initializes a new instance of the {@link RevokedCertificate} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: RevokedCertificateParameters = {}) {
    super();

    this.userCertificate = pvutils.getParametersValue(parameters, USER_CERTIFICATE, RevokedCertificate.defaultValues(USER_CERTIFICATE));
    this.revocationDate = pvutils.getParametersValue(parameters, REVOCATION_DATE, RevokedCertificate.defaultValues(REVOCATION_DATE));
    if (CRL_ENTRY_EXTENSIONS in parameters) {
      this.crlEntryExtensions = pvutils.getParametersValue(parameters, CRL_ENTRY_EXTENSIONS, RevokedCertificate.defaultValues(CRL_ENTRY_EXTENSIONS));
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
  public static override defaultValues(memberName: typeof USER_CERTIFICATE): asn1js.Integer;
  public static override defaultValues(memberName: typeof REVOCATION_DATE): Time;
  public static override defaultValues(memberName: typeof CRL_ENTRY_EXTENSIONS): Extensions;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case USER_CERTIFICATE:
        return new asn1js.Integer();
      case REVOCATION_DATE:
        return new Time();
      case CRL_ENTRY_EXTENSIONS:
        return new Extensions();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * revokedCertificates     SEQUENCE OF SEQUENCE  {
     *        userCertificate         CertificateSerialNumber,
     *        revocationDate          Time,
     *        crlEntryExtensions      Extensions OPTIONAL
     *                                 -- if present, version MUST be v2
     *                             }  OPTIONAL,
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    userCertificate?: string;
    revocationDate?: string;
    crlEntryExtensions?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.userCertificate || USER_CERTIFICATE) }),
        Time.schema({
          names: {
            utcTimeName: (names.revocationDate || REVOCATION_DATE),
            generalTimeName: (names.revocationDate || REVOCATION_DATE)
          }
        }),
        Extensions.schema({
          names: {
            blockName: (names.crlEntryExtensions || CRL_ENTRY_EXTENSIONS)
          }
        }, true)
      ]
    });
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      RevokedCertificate.schema()
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.userCertificate = asn1.result.userCertificate;
    this.revocationDate = new Time({ schema: asn1.result.revocationDate });
    if (CRL_ENTRY_EXTENSIONS in asn1.result) {
      this.crlEntryExtensions = new Extensions({ schema: asn1.result.crlEntryExtensions });
    }
  }

  public toSchema(): asn1js.Sequence {
    // Create array for output sequence
    const outputArray: any[] = [
      this.userCertificate,
      this.revocationDate.toSchema()
    ];
    if (this.crlEntryExtensions) {
      outputArray.push(this.crlEntryExtensions.toSchema());
    }

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toJSON(): RevokedCertificateJson {
    const res: RevokedCertificateJson = {
      userCertificate: this.userCertificate.toJSON(),
      revocationDate: this.revocationDate.toJSON(),
    };

    if (this.crlEntryExtensions) {
      res.crlEntryExtensions = this.crlEntryExtensions.toJSON();
    }

    return res;
  }

}
