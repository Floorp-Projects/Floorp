import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "./AlgorithmIdentifier";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const KEY_INFO = "keyInfo";
const ENTITY_U_INFO = "entityUInfo";
const SUPP_PUB_INFO = "suppPubInfo";
const CLEAR_PROPS = [
  KEY_INFO,
  ENTITY_U_INFO,
  SUPP_PUB_INFO
];

export interface IECCCMSSharedInfo {
  keyInfo: AlgorithmIdentifier;
  entityUInfo?: asn1js.OctetString;
  suppPubInfo: asn1js.OctetString;
}

export interface ECCCMSSharedInfoJson {
  keyInfo: AlgorithmIdentifierJson;
  entityUInfo?: asn1js.OctetStringJson;
  suppPubInfo: asn1js.OctetStringJson;
}

export type ECCCMSSharedInfoParameters = PkiObjectParameters & Partial<IECCCMSSharedInfo>;

/**
 * Represents the ECCCMSSharedInfo structure described in [RFC6318](https://datatracker.ietf.org/doc/html/rfc6318)
 */
export class ECCCMSSharedInfo extends PkiObject implements IECCCMSSharedInfo {

  public static override CLASS_NAME = "ECCCMSSharedInfo";

  public keyInfo!: AlgorithmIdentifier;
  public entityUInfo?: asn1js.OctetString;
  public suppPubInfo!: asn1js.OctetString;

  /**
   * Initializes a new instance of the {@link ECCCMSSharedInfo} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: ECCCMSSharedInfoParameters = {}) {
    super();

    this.keyInfo = pvutils.getParametersValue(parameters, KEY_INFO, ECCCMSSharedInfo.defaultValues(KEY_INFO));
    if (ENTITY_U_INFO in parameters) {
      this.entityUInfo = pvutils.getParametersValue(parameters, ENTITY_U_INFO, ECCCMSSharedInfo.defaultValues(ENTITY_U_INFO));
    }
    this.suppPubInfo = pvutils.getParametersValue(parameters, SUPP_PUB_INFO, ECCCMSSharedInfo.defaultValues(SUPP_PUB_INFO));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof KEY_INFO): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof ENTITY_U_INFO): asn1js.OctetString;
  public static override defaultValues(memberName: typeof SUPP_PUB_INFO): asn1js.OctetString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case KEY_INFO:
        return new AlgorithmIdentifier();
      case ENTITY_U_INFO:
        return new asn1js.OctetString();
      case SUPP_PUB_INFO:
        return new asn1js.OctetString();
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
      case KEY_INFO:
      case ENTITY_U_INFO:
      case SUPP_PUB_INFO:
        return (memberValue.isEqual(ECCCMSSharedInfo.defaultValues(memberName as typeof SUPP_PUB_INFO)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * ECC-CMS-SharedInfo ::= SEQUENCE {
   *    keyInfo      AlgorithmIdentifier,
   *    entityUInfo  [0] EXPLICIT OCTET STRING OPTIONAL,
   *    suppPubInfo  [2] EXPLICIT OCTET STRING }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    keyInfo?: AlgorithmIdentifierSchema;
    entityUInfo?: string;
    suppPubInfo?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AlgorithmIdentifier.schema(names.keyInfo || {}),
        new asn1js.Constructed({
          name: (names.entityUInfo || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          optional: true,
          value: [new asn1js.OctetString()]
        }),
        new asn1js.Constructed({
          name: (names.suppPubInfo || EMPTY_STRING),
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          },
          value: [new asn1js.OctetString()]
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
      ECCCMSSharedInfo.schema({
        names: {
          keyInfo: {
            names: {
              blockName: KEY_INFO
            }
          },
          entityUInfo: ENTITY_U_INFO,
          suppPubInfo: SUPP_PUB_INFO
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.keyInfo = new AlgorithmIdentifier({ schema: asn1.result.keyInfo });
    if (ENTITY_U_INFO in asn1.result)
      this.entityUInfo = asn1.result.entityUInfo.valueBlock.value[0];
    this.suppPubInfo = asn1.result.suppPubInfo.valueBlock.value[0];
  }

  public toSchema(): asn1js.Sequence {
    //#region Create output array for sequence
    const outputArray = [];

    outputArray.push(this.keyInfo.toSchema());

    if (this.entityUInfo) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [this.entityUInfo]
      }));
    }

    outputArray.push(new asn1js.Constructed({
      idBlock: {
        tagClass: 3, // CONTEXT-SPECIFIC
        tagNumber: 2 // [2]
      },
      value: [this.suppPubInfo]
    }));
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return new asn1js.Sequence({
      value: outputArray
    });
    //#endregion
  }

  public toJSON(): ECCCMSSharedInfoJson {
    const res: ECCCMSSharedInfoJson = {
      keyInfo: this.keyInfo.toJSON(),
      suppPubInfo: this.suppPubInfo.toJSON(),
    };

    if (this.entityUInfo) {
      res.entityUInfo = this.entityUInfo.toJSON();
    }

    return res;
  }

}
