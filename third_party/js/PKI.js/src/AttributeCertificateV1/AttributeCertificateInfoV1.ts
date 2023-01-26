import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { GeneralNames, GeneralNamesJson } from "../GeneralNames";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "../AlgorithmIdentifier";
import { Attribute, AttributeJson } from "../Attribute";
import { Extensions, ExtensionsJson, ExtensionsSchema } from "../Extensions";
import { AttCertValidityPeriod, AttCertValidityPeriodJson, AttCertValidityPeriodSchema } from "./AttCertValidityPeriod";
import { IssuerSerial, IssuerSerialJson } from "./IssuerSerial";
import * as Schema from "../Schema";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import { AsnError } from "../errors";
import { EMPTY_STRING } from "../constants";

const VERSION = "version";
const BASE_CERTIFICATE_ID = "baseCertificateID";
const SUBJECT_NAME = "subjectName";
const ISSUER = "issuer";
const SIGNATURE = "signature";
const SERIAL_NUMBER = "serialNumber";
const ATTR_CERT_VALIDITY_PERIOD = "attrCertValidityPeriod";
const ATTRIBUTES = "attributes";
const ISSUER_UNIQUE_ID = "issuerUniqueID";
const EXTENSIONS = "extensions";
const CLEAR_PROPS = [
  VERSION,
  BASE_CERTIFICATE_ID,
  SUBJECT_NAME,
  ISSUER,
  SIGNATURE,
  SERIAL_NUMBER,
  ATTR_CERT_VALIDITY_PERIOD,
  ATTRIBUTES,
  ISSUER_UNIQUE_ID,
  EXTENSIONS,
];

export interface IAttributeCertificateInfoV1 {
  /**
   * The version field MUST have the value of v2
   */
  version: number;
  baseCertificateID?: IssuerSerial;
  subjectName?: GeneralNames;
  issuer: GeneralNames;
  /**
   * Contains the algorithm identifier used to validate the AC signature
   */
  signature: AlgorithmIdentifier;
  serialNumber: asn1js.Integer;
  /**
   * Specifies the period for which the AC issuer certifies that the binding between
   * the holder and the attributes fields will be valid
   */
  attrCertValidityPeriod: AttCertValidityPeriod;
  /**
   * The attributes field gives information about the AC holder
   */
  attributes: Attribute[];

  /**
   * Issuer unique identifier
   */
  issuerUniqueID?: asn1js.BitString;
  /**
   * The extensions field generally gives information about the AC as opposed
   * to information about the AC holder
   */
  extensions?: Extensions;
}

export interface AttributeCertificateInfoV1Json {
  version: number;
  baseCertificateID?: IssuerSerialJson;
  subjectName?: GeneralNamesJson;
  issuer: GeneralNamesJson;
  signature: AlgorithmIdentifierJson;
  serialNumber: asn1js.IntegerJson;
  attrCertValidityPeriod: AttCertValidityPeriodJson;
  attributes: AttributeJson[];
  issuerUniqueID: asn1js.BitStringJson;
  extensions: ExtensionsJson;
}

export type AttributeCertificateInfoV1Parameters = PkiObjectParameters & Partial<IAttributeCertificateInfoV1>;

export type AttributeCertificateInfoV1Schema = Schema.SchemaParameters<{
  version?: string;
  baseCertificateID?: string;
  subjectName?: string;
  signature?: AlgorithmIdentifierSchema;
  issuer?: string;
  attrCertValidityPeriod?: AttCertValidityPeriodSchema;
  serialNumber?: string;
  attributes?: string;
  issuerUniqueID?: string;
  extensions?: ExtensionsSchema;
}>;

/**
 * Represents the AttributeCertificateInfoV1 structure described in [RFC5755](https://datatracker.ietf.org/doc/html/rfc5755)
 */
export class AttributeCertificateInfoV1 extends PkiObject implements IAttributeCertificateInfoV1 {

  public static override CLASS_NAME = "AttributeCertificateInfoV1";

  version!: number;
  baseCertificateID?: IssuerSerial;
  subjectName?: GeneralNames;
  issuer!: GeneralNames;
  signature!: AlgorithmIdentifier;
  serialNumber!: asn1js.Integer;
  attrCertValidityPeriod!: AttCertValidityPeriod;
  attributes!: Attribute[];
  issuerUniqueID?: asn1js.BitString;
  extensions?: Extensions;

  /**
   * Initializes a new instance of the {@link AttributeCertificateInfoV1} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AttributeCertificateInfoV1Parameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, AttributeCertificateInfoV1.defaultValues(VERSION));
    if (BASE_CERTIFICATE_ID in parameters) {
      this.baseCertificateID = pvutils.getParametersValue(parameters, BASE_CERTIFICATE_ID, AttributeCertificateInfoV1.defaultValues(BASE_CERTIFICATE_ID));
    }
    if (SUBJECT_NAME in parameters) {
      this.subjectName = pvutils.getParametersValue(parameters, SUBJECT_NAME, AttributeCertificateInfoV1.defaultValues(SUBJECT_NAME));
    }
    this.issuer = pvutils.getParametersValue(parameters, ISSUER, AttributeCertificateInfoV1.defaultValues(ISSUER));
    this.signature = pvutils.getParametersValue(parameters, SIGNATURE, AttributeCertificateInfoV1.defaultValues(SIGNATURE));
    this.serialNumber = pvutils.getParametersValue(parameters, SERIAL_NUMBER, AttributeCertificateInfoV1.defaultValues(SERIAL_NUMBER));
    this.attrCertValidityPeriod = pvutils.getParametersValue(parameters, ATTR_CERT_VALIDITY_PERIOD, AttributeCertificateInfoV1.defaultValues(ATTR_CERT_VALIDITY_PERIOD));
    this.attributes = pvutils.getParametersValue(parameters, ATTRIBUTES, AttributeCertificateInfoV1.defaultValues(ATTRIBUTES));
    if (ISSUER_UNIQUE_ID in parameters)
      this.issuerUniqueID = pvutils.getParametersValue(parameters, ISSUER_UNIQUE_ID, AttributeCertificateInfoV1.defaultValues(ISSUER_UNIQUE_ID));

    if (EXTENSIONS in parameters) {
      this.extensions = pvutils.getParametersValue(parameters, EXTENSIONS, AttributeCertificateInfoV1.defaultValues(EXTENSIONS));
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
  public static override defaultValues(memberName: typeof VERSION): number;
  public static override defaultValues(memberName: typeof BASE_CERTIFICATE_ID): IssuerSerial;
  public static override defaultValues(memberName: typeof SUBJECT_NAME): GeneralNames;
  public static override defaultValues(memberName: typeof ISSUER): GeneralNames;
  public static override defaultValues(memberName: typeof SIGNATURE): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SERIAL_NUMBER): asn1js.Integer;
  public static override defaultValues(memberName: typeof ATTR_CERT_VALIDITY_PERIOD): AttCertValidityPeriod;
  public static override defaultValues(memberName: typeof ATTRIBUTES): Attribute[];
  public static override defaultValues(memberName: typeof ISSUER_UNIQUE_ID): asn1js.BitString;
  public static override defaultValues(memberName: typeof EXTENSIONS): Extensions;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 0;
      case BASE_CERTIFICATE_ID:
        return new IssuerSerial();
      case SUBJECT_NAME:
        return new GeneralNames();
      case ISSUER:
        return new GeneralNames();
      case SIGNATURE:
        return new AlgorithmIdentifier();
      case SERIAL_NUMBER:
        return new asn1js.Integer();
      case ATTR_CERT_VALIDITY_PERIOD:
        return new AttCertValidityPeriod();
      case ATTRIBUTES:
        return [];
      case ISSUER_UNIQUE_ID:
        return new asn1js.BitString();
      case EXTENSIONS:
        return new Extensions();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AttributeCertificateInfo ::= SEQUENCE {
   *   version Version DEFAULT v1,
   *   subject CHOICE {
   *     baseCertificateID [0] IssuerSerial, -- associated with a Public Key Certificate
   *     subjectName [1] GeneralNames }, -- associated with a name
   *   issuer GeneralNames, -- CA issuing the attribute certificate
   *   signature AlgorithmIdentifier,
   *   serialNumber CertificateSerialNumber,
   *   attrCertValidityPeriod AttCertValidityPeriod,
   *   attributes SEQUENCE OF Attribute,
   *   issuerUniqueID UniqueIdentifier OPTIONAL,
   *   extensions Extensions OPTIONAL
   * }
   *```
   */
  public static override schema(parameters: AttributeCertificateInfoV1Schema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        new asn1js.Choice({
          value: [
            new asn1js.Constructed({
              name: (names.baseCertificateID || EMPTY_STRING),
              idBlock: {
                tagClass: 3,
                tagNumber: 0 // [0]
              },
              value: IssuerSerial.schema().valueBlock.value
            }),
            new asn1js.Constructed({
              name: (names.subjectName || EMPTY_STRING),
              idBlock: {
                tagClass: 3,
                tagNumber: 1 // [2]
              },
              value: GeneralNames.schema().valueBlock.value
            }),
          ]
        }),
        GeneralNames.schema({
          names: {
            blockName: (names.issuer || EMPTY_STRING)
          }
        }),
        AlgorithmIdentifier.schema(names.signature || {}),
        new asn1js.Integer({ name: (names.serialNumber || EMPTY_STRING) }),
        AttCertValidityPeriod.schema(names.attrCertValidityPeriod || {}),
        new asn1js.Sequence({
          name: (names.attributes || EMPTY_STRING),
          value: [
            new asn1js.Repeated({
              value: Attribute.schema()
            })
          ]
        }),
        new asn1js.BitString({
          optional: true,
          name: (names.issuerUniqueID || EMPTY_STRING)
        }),
        Extensions.schema(names.extensions || {}, true)
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);
    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      AttributeCertificateInfoV1.schema({
        names: {
          version: VERSION,
          baseCertificateID: BASE_CERTIFICATE_ID,
          subjectName: SUBJECT_NAME,
          issuer: ISSUER,
          signature: {
            names: {
              blockName: SIGNATURE
            }
          },
          serialNumber: SERIAL_NUMBER,
          attrCertValidityPeriod: {
            names: {
              blockName: ATTR_CERT_VALIDITY_PERIOD
            }
          },
          attributes: ATTRIBUTES,
          issuerUniqueID: ISSUER_UNIQUE_ID,
          extensions: {
            names: {
              blockName: EXTENSIONS
            }
          }
        }
      })
    );

    AsnError.assertSchema(asn1, this.className);
    //#endregion

    //#region Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;

    if (BASE_CERTIFICATE_ID in asn1.result) {
      this.baseCertificateID = new IssuerSerial({
        schema: new asn1js.Sequence({
          value: asn1.result.baseCertificateID.valueBlock.value
        })
      });
    }

    if (SUBJECT_NAME in asn1.result) {
      this.subjectName = new GeneralNames({
        schema: new asn1js.Sequence({
          value: asn1.result.subjectName.valueBlock.value
        })
      });
    }

    this.issuer = asn1.result.issuer;
    this.signature = new AlgorithmIdentifier({ schema: asn1.result.signature });
    this.serialNumber = asn1.result.serialNumber;
    this.attrCertValidityPeriod = new AttCertValidityPeriod({ schema: asn1.result.attrCertValidityPeriod });
    this.attributes = Array.from(asn1.result.attributes.valueBlock.value, element => new Attribute({ schema: element }));

    if (ISSUER_UNIQUE_ID in asn1.result) {
      this.issuerUniqueID = asn1.result.issuerUniqueID;
    }

    if (EXTENSIONS in asn1.result) {
      this.extensions = new Extensions({ schema: asn1.result.extensions });
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    const result = new asn1js.Sequence({
      value: [new asn1js.Integer({ value: this.version })]
    });

    if (this.baseCertificateID) {
      result.valueBlock.value.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 0 // [0]
        },
        value: this.baseCertificateID.toSchema().valueBlock.value
      }));
    }

    if (this.subjectName) {
      result.valueBlock.value.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3,
          tagNumber: 1 // [1]
        },
        value: this.subjectName.toSchema().valueBlock.value
      }));
    }

    result.valueBlock.value.push(this.issuer.toSchema());
    result.valueBlock.value.push(this.signature.toSchema());
    result.valueBlock.value.push(this.serialNumber);
    result.valueBlock.value.push(this.attrCertValidityPeriod.toSchema());
    result.valueBlock.value.push(new asn1js.Sequence({
      value: Array.from(this.attributes, o => o.toSchema())
    }));

    if (this.issuerUniqueID) {
      result.valueBlock.value.push(this.issuerUniqueID);
    }

    if (this.extensions) {
      result.valueBlock.value.push(this.extensions.toSchema());
    }

    return result;
  }

  public toJSON(): AttributeCertificateInfoV1Json {
    const result = {
      version: this.version
    } as AttributeCertificateInfoV1Json;

    if (this.baseCertificateID) {
      result.baseCertificateID = this.baseCertificateID.toJSON();
    }

    if (this.subjectName) {
      result.subjectName = this.subjectName.toJSON();
    }

    result.issuer = this.issuer.toJSON();
    result.signature = this.signature.toJSON();
    result.serialNumber = this.serialNumber.toJSON();
    result.attrCertValidityPeriod = this.attrCertValidityPeriod.toJSON();
    result.attributes = Array.from(this.attributes, o => o.toJSON());

    if (this.issuerUniqueID) {
      result.issuerUniqueID = this.issuerUniqueID.toJSON();
    }

    if (this.extensions) {
      result.extensions = this.extensions.toJSON();
    }

    return result;
  }

}
