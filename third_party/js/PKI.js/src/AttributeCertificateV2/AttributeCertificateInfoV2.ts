import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { GeneralNames, GeneralNamesJson } from "../GeneralNames";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "../AlgorithmIdentifier";
import { Attribute, AttributeJson } from "../Attribute";
import { Extensions, ExtensionsJson, ExtensionsSchema } from "../Extensions";
import { AttCertValidityPeriod, AttCertValidityPeriodJson, AttCertValidityPeriodSchema } from "../AttributeCertificateV1";
import { V2Form, V2FormJson } from "./V2Form";
import { Holder, HolderJson, HolderSchema } from "./Holder";
import * as Schema from "../Schema";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import { AsnError } from "../errors";
import { EMPTY_STRING } from "../constants";

const VERSION = "version";
const HOLDER = "holder";
const ISSUER = "issuer";
const SIGNATURE = "signature";
const SERIAL_NUMBER = "serialNumber";
const ATTR_CERT_VALIDITY_PERIOD = "attrCertValidityPeriod";
const ATTRIBUTES = "attributes";
const ISSUER_UNIQUE_ID = "issuerUniqueID";
const EXTENSIONS = "extensions";
const CLEAR_PROPS = [
  VERSION,
  HOLDER,
  ISSUER,
  SIGNATURE,
  SERIAL_NUMBER,
  ATTR_CERT_VALIDITY_PERIOD,
  ATTRIBUTES,
  ISSUER_UNIQUE_ID,
  EXTENSIONS
];

export interface IAttributeCertificateInfoV2 {
  version: number;
  holder: Holder;
  issuer: GeneralNames | V2Form;
  signature: AlgorithmIdentifier;
  serialNumber: asn1js.Integer;
  attrCertValidityPeriod: AttCertValidityPeriod;
  attributes: Attribute[];
  issuerUniqueID?: asn1js.BitString;
  extensions?: Extensions;
}

export type AttributeCertificateInfoV2Parameters = PkiObjectParameters & Partial<AttributeCertificateInfoV2>;

export type AttributeCertificateInfoV2Schema = Schema.SchemaParameters<{
  version?: string;
  holder?: HolderSchema;
  issuer?: string;
  signature?: AlgorithmIdentifierSchema;
  serialNumber?: string;
  attrCertValidityPeriod?: AttCertValidityPeriodSchema;
  attributes?: string;
  issuerUniqueID?: string;
  extensions?: ExtensionsSchema;
}>;

export interface AttributeCertificateInfoV2Json {
  version: number;
  holder: HolderJson;
  issuer: GeneralNamesJson | V2FormJson;
  signature: AlgorithmIdentifierJson;
  serialNumber: asn1js.IntegerJson;
  attrCertValidityPeriod: AttCertValidityPeriodJson;
  attributes: AttributeJson[];
  issuerUniqueID?: asn1js.BitStringJson;
  extensions?: ExtensionsJson;
}

/**
 * Represents the AttributeCertificateInfoV2 structure described in [RFC5755](https://datatracker.ietf.org/doc/html/rfc5755)
 */
export class AttributeCertificateInfoV2 extends PkiObject implements IAttributeCertificateInfoV2 {

  public static override CLASS_NAME = "AttributeCertificateInfoV2";

  public version!: number;
  public holder!: Holder;
  public issuer!: GeneralNames | V2Form;
  public signature!: AlgorithmIdentifier;
  public serialNumber!: asn1js.Integer;
  public attrCertValidityPeriod!: AttCertValidityPeriod;
  public attributes!: Attribute[];
  public issuerUniqueID?: asn1js.BitString;
  public extensions?: Extensions;

  /**
   * Initializes a new instance of the {@link AttributeCertificateInfoV2} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AttributeCertificateInfoV2Parameters = {}) {
    super();

    this.version = pvutils.getParametersValue(parameters, VERSION, AttributeCertificateInfoV2.defaultValues(VERSION));
    this.holder = pvutils.getParametersValue(parameters, HOLDER, AttributeCertificateInfoV2.defaultValues(HOLDER));
    this.issuer = pvutils.getParametersValue(parameters, ISSUER, AttributeCertificateInfoV2.defaultValues(ISSUER));
    this.signature = pvutils.getParametersValue(parameters, SIGNATURE, AttributeCertificateInfoV2.defaultValues(SIGNATURE));
    this.serialNumber = pvutils.getParametersValue(parameters, SERIAL_NUMBER, AttributeCertificateInfoV2.defaultValues(SERIAL_NUMBER));
    this.attrCertValidityPeriod = pvutils.getParametersValue(parameters, ATTR_CERT_VALIDITY_PERIOD, AttributeCertificateInfoV2.defaultValues(ATTR_CERT_VALIDITY_PERIOD));
    this.attributes = pvutils.getParametersValue(parameters, ATTRIBUTES, AttributeCertificateInfoV2.defaultValues(ATTRIBUTES));
    if (ISSUER_UNIQUE_ID in parameters) {
      this.issuerUniqueID = pvutils.getParametersValue(parameters, ISSUER_UNIQUE_ID, AttributeCertificateInfoV2.defaultValues(ISSUER_UNIQUE_ID));
    }
    if (EXTENSIONS in parameters) {
      this.extensions = pvutils.getParametersValue(parameters, EXTENSIONS, AttributeCertificateInfoV2.defaultValues(EXTENSIONS));
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
  public static override defaultValues(memberName: typeof HOLDER): Holder;
  public static override defaultValues(memberName: typeof ISSUER): GeneralNames | V2Form;
  public static override defaultValues(memberName: typeof SIGNATURE): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SERIAL_NUMBER): asn1js.Integer;
  public static override defaultValues(memberName: typeof ATTR_CERT_VALIDITY_PERIOD): AttCertValidityPeriod;
  public static override defaultValues(memberName: typeof ATTRIBUTES): Attribute[];
  public static override defaultValues(memberName: typeof ISSUER_UNIQUE_ID): asn1js.BitString;
  public static override defaultValues(memberName: typeof EXTENSIONS): Extensions;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case VERSION:
        return 1;
      case HOLDER:
        return new Holder();
      case ISSUER:
        return {};
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
   * AttributeCertificateInfoV2 ::= SEQUENCE {
   *   version                 AttCertVersion, -- version is v2
   *   holder                  Holder,
   *   issuer                  AttCertIssuer,
   *   signature               AlgorithmIdentifier,
   *   serialNumber            CertificateSerialNumber,
   *   attrCertValidityPeriod  AttCertValidityPeriod,
   *   attributes              SEQUENCE OF Attribute,
   *   issuerUniqueID          UniqueIdentifier OPTIONAL,
   *   extensions              Extensions OPTIONAL
   * }
   *```
   */
  public static override schema(parameters: AttributeCertificateInfoV2Schema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.version || EMPTY_STRING) }),
        Holder.schema(names.holder || {}),
        new asn1js.Choice({
          value: [
            GeneralNames.schema({
              names: {
                blockName: (names.issuer || EMPTY_STRING)
              }
            }),
            new asn1js.Constructed({
              name: (names.issuer || EMPTY_STRING),
              idBlock: {
                tagClass: 3,
                tagNumber: 0 // [0]
              },
              value: V2Form.schema().valueBlock.value
            })
          ]
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

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      AttributeCertificateInfoV2.schema({
        names: {
          version: VERSION,
          holder: {
            names: {
              blockName: HOLDER
            }
          },
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

    //#region Get internal properties from parsed schema
    this.version = asn1.result.version.valueBlock.valueDec;
    this.holder = new Holder({ schema: asn1.result.holder });

    switch (asn1.result.issuer.idBlock.tagClass) {
      case 3: // V2Form
        this.issuer = new V2Form({
          schema: new asn1js.Sequence({
            value: asn1.result.issuer.valueBlock.value
          })
        });
        break;
      case 1: // GeneralNames (should not be used)
      default:
        throw new Error("Incorrect value for 'issuer' in AttributeCertificateInfoV2");
    }

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
      value: [
        new asn1js.Integer({ value: this.version }),
        this.holder.toSchema(),
        new asn1js.Constructed({
          idBlock: {
            tagClass: 3,
            tagNumber: 0 // [0]
          },
          value: this.issuer.toSchema().valueBlock.value
        }),
        this.signature.toSchema(),
        this.serialNumber,
        this.attrCertValidityPeriod.toSchema(),
        new asn1js.Sequence({
          value: Array.from(this.attributes, o => o.toSchema())
        })
      ]
    });

    if (this.issuerUniqueID) {
      result.valueBlock.value.push(this.issuerUniqueID);
    }
    if (this.extensions) {
      result.valueBlock.value.push(this.extensions.toSchema());
    }

    return result;
  }

  public toJSON(): AttributeCertificateInfoV2Json {
    const result: AttributeCertificateInfoV2Json = {
      version: this.version,
      holder: this.holder.toJSON(),
      issuer: this.issuer.toJSON(),
      signature: this.signature.toJSON(),
      serialNumber: this.serialNumber.toJSON(),
      attrCertValidityPeriod: this.attrCertValidityPeriod.toJSON(),
      attributes: Array.from(this.attributes, o => o.toJSON())
    };

    if (this.issuerUniqueID) {
      result.issuerUniqueID = this.issuerUniqueID.toJSON();
    }
    if (this.extensions) {
      result.extensions = this.extensions.toJSON();
    }

    return result;
  }

}
