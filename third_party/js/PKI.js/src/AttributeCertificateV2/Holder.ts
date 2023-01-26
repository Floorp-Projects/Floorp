import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { GeneralNames, GeneralNamesJson } from "../GeneralNames";
import { IssuerSerial, IssuerSerialJson } from "../AttributeCertificateV1";
import { ObjectDigestInfo, ObjectDigestInfoJson } from "./ObjectDigestInfo";
import * as Schema from "../Schema";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import { AsnError } from "../errors";
import { EMPTY_STRING } from "../constants";

const BASE_CERTIFICATE_ID = "baseCertificateID";
const ENTITY_NAME = "entityName";
const OBJECT_DIGEST_INFO = "objectDigestInfo";
const CLEAR_PROPS = [
  BASE_CERTIFICATE_ID,
  ENTITY_NAME,
  OBJECT_DIGEST_INFO
];

export interface IHolder {
  baseCertificateID?: IssuerSerial;
  entityName?: GeneralNames;
  objectDigestInfo?: ObjectDigestInfo;
}

export type HolderParameters = PkiObjectParameters & Partial<IHolder>;

export type HolderSchema = Schema.SchemaParameters<{
  baseCertificateID?: string;
  entityName?: string;
  objectDigestInfo?: string;
}>;

export interface HolderJson {
  baseCertificateID?: IssuerSerialJson;
  entityName?: GeneralNamesJson;
  objectDigestInfo?: ObjectDigestInfoJson;
}

/**
 * Represents the Holder structure described in [RFC5755](https://datatracker.ietf.org/doc/html/rfc5755)
 */
export class Holder extends PkiObject implements IHolder {

  public static override CLASS_NAME = "Holder";

  public baseCertificateID?: IssuerSerial;
  public entityName?: GeneralNames;
  public objectDigestInfo?: ObjectDigestInfo;

  /**
   * Initializes a new instance of the {@link AttributeCertificateInfoV1} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: HolderParameters = {}) {
    super();

    if (BASE_CERTIFICATE_ID in parameters) {
      this.baseCertificateID = pvutils.getParametersValue(parameters, BASE_CERTIFICATE_ID, Holder.defaultValues(BASE_CERTIFICATE_ID));
    }
    if (ENTITY_NAME in parameters) {
      this.entityName = pvutils.getParametersValue(parameters, ENTITY_NAME, Holder.defaultValues(ENTITY_NAME));
    }
    if (OBJECT_DIGEST_INFO in parameters) {
      this.objectDigestInfo = pvutils.getParametersValue(parameters, OBJECT_DIGEST_INFO, Holder.defaultValues(OBJECT_DIGEST_INFO));
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
  public static override defaultValues(memberName: typeof BASE_CERTIFICATE_ID): IssuerSerial;
  public static override defaultValues(memberName: typeof ENTITY_NAME): GeneralNames;
  public static override defaultValues(memberName: typeof OBJECT_DIGEST_INFO): ObjectDigestInfo;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case BASE_CERTIFICATE_ID:
        return new IssuerSerial();
      case ENTITY_NAME:
        return new GeneralNames();
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
   * Holder ::= SEQUENCE {
   *   baseCertificateID   [0] IssuerSerial OPTIONAL,
   *       -- the issuer and serial number of
   *       -- the holder's Public Key Certificate
   *   entityName          [1] GeneralNames OPTIONAL,
   *       -- the name of the claimant or role
   *   objectDigestInfo    [2] ObjectDigestInfo OPTIONAL
   *       -- used to directly authenticate the holder,
   *       -- for example, an executable
   * }
   *```
   */
  public static override schema(parameters: HolderSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
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
          name: (names.entityName || EMPTY_STRING),
          idBlock: {
            tagClass: 3,
            tagNumber: 1 // [2]
          },
          value: GeneralNames.schema().valueBlock.value
        }),
        new asn1js.Constructed({
          optional: true,
          name: (names.objectDigestInfo || EMPTY_STRING),
          idBlock: {
            tagClass: 3,
            tagNumber: 2 // [2]
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
      Holder.schema({
        names: {
          baseCertificateID: BASE_CERTIFICATE_ID,
          entityName: ENTITY_NAME,
          objectDigestInfo: OBJECT_DIGEST_INFO
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (BASE_CERTIFICATE_ID in asn1.result) {
      this.baseCertificateID = new IssuerSerial({
        schema: new asn1js.Sequence({
          value: asn1.result.baseCertificateID.valueBlock.value
        })
      });
    }
    if (ENTITY_NAME in asn1.result) {
      this.entityName = new GeneralNames({
        schema: new asn1js.Sequence({
          value: asn1.result.entityName.valueBlock.value
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
  }

  public toSchema(): asn1js.Sequence {
    const result = new asn1js.Sequence();

    if (this.baseCertificateID) {
      result.valueBlock.value.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 0 // [0]
        },
        value: this.baseCertificateID.toSchema().valueBlock.value
      }));
    }

    if (this.entityName) {
      result.valueBlock.value.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 1 // [1]
        },
        value: this.entityName.toSchema().valueBlock.value
      }));
    }

    if (this.objectDigestInfo) {
      result.valueBlock.value.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 2 // [2]
        },
        value: this.objectDigestInfo.toSchema().valueBlock.value
      }));
    }

    return result;
  }

  public toJSON(): HolderJson {
    const result: HolderJson = {};

    if (this.baseCertificateID) {
      result.baseCertificateID = this.baseCertificateID.toJSON();
    }

    if (this.entityName) {
      result.entityName = this.entityName.toJSON();
    }

    if (this.objectDigestInfo) {
      result.objectDigestInfo = this.objectDigestInfo.toJSON();
    }

    return result;
  }

}
