import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { GeneralNames, GeneralNamesJson } from "../GeneralNames";
import { IssuerSerial, IssuerSerialJson } from "../AttributeCertificateV1";
import { ObjectDigestInfo, ObjectDigestInfoJson } from "./ObjectDigestInfo";
import * as Schema from "../Schema";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import { AsnError } from "../errors";
import { EMPTY_STRING } from "../constants";

const ISSUER_NAME = "issuerName";
const BASE_CERTIFICATE_ID = "baseCertificateID";
const OBJECT_DIGEST_INFO = "objectDigestInfo";
const CLEAR_PROPS = [
  ISSUER_NAME,
  BASE_CERTIFICATE_ID,
  OBJECT_DIGEST_INFO
];

export interface IV2Form {
  issuerName?: GeneralNames;
  baseCertificateID?: IssuerSerial;
  objectDigestInfo?: ObjectDigestInfo;
}

export type V2FormParameters = PkiObjectParameters & Partial<IV2Form>;

export interface V2FormJson {
  issuerName?: GeneralNamesJson;
  baseCertificateID?: IssuerSerialJson;
  objectDigestInfo?: ObjectDigestInfoJson;
}

/**
 * Represents the V2Form structure described in [RFC5755](https://datatracker.ietf.org/doc/html/rfc5755)
 */
export class V2Form extends PkiObject implements IV2Form {

  public static override CLASS_NAME = "V2Form";

  public issuerName?: GeneralNames;
  public baseCertificateID?: IssuerSerial;
  public objectDigestInfo?: ObjectDigestInfo;

  /**
   * Initializes a new instance of the {@link V2Form} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: V2FormParameters = {}) {
    super();

    if (ISSUER_NAME in parameters) {
      this.issuerName = pvutils.getParametersValue(parameters, ISSUER_NAME, V2Form.defaultValues(ISSUER_NAME));
    }
    if (BASE_CERTIFICATE_ID in parameters) {
      this.baseCertificateID = pvutils.getParametersValue(parameters, BASE_CERTIFICATE_ID, V2Form.defaultValues(BASE_CERTIFICATE_ID));
    }
    if (OBJECT_DIGEST_INFO in parameters) {
      this.objectDigestInfo = pvutils.getParametersValue(parameters, OBJECT_DIGEST_INFO, V2Form.defaultValues(OBJECT_DIGEST_INFO));
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
  public static override defaultValues(memberName: typeof ISSUER_NAME): GeneralNames;
  public static override defaultValues(memberName: typeof BASE_CERTIFICATE_ID): IssuerSerial;
  public static override defaultValues(memberName: typeof OBJECT_DIGEST_INFO): ObjectDigestInfo;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ISSUER_NAME:
        return new GeneralNames();
      case BASE_CERTIFICATE_ID:
        return new IssuerSerial();
      case OBJECT_DIGEST_INFO:
        return new ObjectDigestInfo();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * V2Form ::= SEQUENCE {
   *   issuerName            GeneralNames  OPTIONAL,
   *   baseCertificateID     [0] IssuerSerial  OPTIONAL,
   *   objectDigestInfo      [1] ObjectDigestInfo  OPTIONAL
   *     -- issuerName MUST be present in this profile
   *     -- baseCertificateID and objectDigestInfo MUST NOT
   *     -- be present in this profile
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    issuerName?: string;
    baseCertificateID?: string;
    objectDigestInfo?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        GeneralNames.schema({
          names: {
            blockName: names.issuerName
          }
        }, true),
        new asn1js.Constructed({
          optional: true,
          name: (names.baseCertificateID || EMPTY_STRING),
          idBlock: {
            tagClass: 3,
            tagNumber: 0 // [0]
          },
          value: IssuerSerial.schema().valueBlock.value
        }),
        new asn1js.Constructed({
          optional: true,
          name: (names.objectDigestInfo || EMPTY_STRING),
          idBlock: {
            tagClass: 3,
            tagNumber: 1 // [1]
          },
          value: ObjectDigestInfo.schema().valueBlock.value
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
      V2Form.schema({
        names: {
          issuerName: ISSUER_NAME,
          baseCertificateID: BASE_CERTIFICATE_ID,
          objectDigestInfo: OBJECT_DIGEST_INFO
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    if (ISSUER_NAME in asn1.result)
      this.issuerName = new GeneralNames({ schema: asn1.result.issuerName });

    if (BASE_CERTIFICATE_ID in asn1.result) {
      this.baseCertificateID = new IssuerSerial({
        schema: new asn1js.Sequence({
          value: asn1.result.baseCertificateID.valueBlock.value
        })
      });
    }

    if (OBJECT_DIGEST_INFO in asn1.result) {
      this.objectDigestInfo = new ObjectDigestInfo({
        schema: new asn1js.Sequence({
          value: asn1.result.objectDigestInfo.valueBlock.value
        })
      });
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    const result = new asn1js.Sequence();

    if (this.issuerName)
      result.valueBlock.value.push(this.issuerName.toSchema());

    if (this.baseCertificateID) {
      result.valueBlock.value.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 0 // [0]
        },
        value: this.baseCertificateID.toSchema().valueBlock.value
      }));
    }

    if (this.objectDigestInfo) {
      result.valueBlock.value.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 1 // [1]
        },
        value: this.objectDigestInfo.toSchema().valueBlock.value
      }));
    }

    return result;
  }

  public toJSON(): V2FormJson {
    const result: V2FormJson = {};

    if (this.issuerName) {
      result.issuerName = this.issuerName.toJSON();
    }
    if (this.baseCertificateID) {
      result.baseCertificateID = this.baseCertificateID.toJSON();
    }
    if (this.objectDigestInfo) {
      result.objectDigestInfo = this.objectDigestInfo.toJSON();
    }

    return result;
  }

}
