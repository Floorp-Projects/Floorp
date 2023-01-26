import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { GeneralName, GeneralNameJson } from "./GeneralName";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RelativeDistinguishedNames, RelativeDistinguishedNamesJson } from "./RelativeDistinguishedNames";
import * as Schema from "./Schema";

const DISTRIBUTION_POINT = "distributionPoint";
const DISTRIBUTION_POINT_NAMES = "distributionPointNames";
const ONLY_CONTAINS_USER_CERTS = "onlyContainsUserCerts";
const ONLY_CONTAINS_CA_CERTS = "onlyContainsCACerts";
const ONLY_SOME_REASON = "onlySomeReasons";
const INDIRECT_CRL = "indirectCRL";
const ONLY_CONTAINS_ATTRIBUTE_CERTS = "onlyContainsAttributeCerts";
const CLEAR_PROPS = [
  DISTRIBUTION_POINT,
  DISTRIBUTION_POINT_NAMES,
  ONLY_CONTAINS_USER_CERTS,
  ONLY_CONTAINS_CA_CERTS,
  ONLY_SOME_REASON,
  INDIRECT_CRL,
  ONLY_CONTAINS_ATTRIBUTE_CERTS,
];

export interface IIssuingDistributionPoint {
  distributionPoint?: DistributionPointName;
  onlyContainsUserCerts: boolean;
  onlyContainsCACerts: boolean;
  onlySomeReasons?: number;
  indirectCRL: boolean;
  onlyContainsAttributeCerts: boolean;
}

export interface IssuingDistributionPointJson {
  distributionPoint?: DistributionPointNameJson;
  onlyContainsUserCerts?: boolean;
  onlyContainsCACerts?: boolean;
  onlySomeReasons?: number;
  indirectCRL?: boolean;
  onlyContainsAttributeCerts?: boolean;
}

export type DistributionPointName = GeneralName[] | RelativeDistinguishedNames;
export type DistributionPointNameJson = GeneralNameJson[] | RelativeDistinguishedNamesJson;

export type IssuingDistributionPointParameters = PkiObjectParameters & Partial<IIssuingDistributionPoint>;

/**
 * Represents the IssuingDistributionPoint structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class IssuingDistributionPoint extends PkiObject implements IIssuingDistributionPoint {

  public static override CLASS_NAME = "IssuingDistributionPoint";

  public distributionPoint?: DistributionPointName;
  public onlyContainsUserCerts!: boolean;
  public onlyContainsCACerts!: boolean;
  public onlySomeReasons?: number;
  public indirectCRL!: boolean;
  public onlyContainsAttributeCerts!: boolean;

  /**
   * Initializes a new instance of the {@link IssuingDistributionPoint} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: IssuingDistributionPointParameters = {}) {
    super();

    if (DISTRIBUTION_POINT in parameters) {
      this.distributionPoint = pvutils.getParametersValue(parameters, DISTRIBUTION_POINT, IssuingDistributionPoint.defaultValues(DISTRIBUTION_POINT));
    }

    this.onlyContainsUserCerts = pvutils.getParametersValue(parameters, ONLY_CONTAINS_USER_CERTS, IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_USER_CERTS));
    this.onlyContainsCACerts = pvutils.getParametersValue(parameters, ONLY_CONTAINS_CA_CERTS, IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_CA_CERTS));
    if (ONLY_SOME_REASON in parameters) {
      this.onlySomeReasons = pvutils.getParametersValue(parameters, ONLY_SOME_REASON, IssuingDistributionPoint.defaultValues(ONLY_SOME_REASON));
    }
    this.indirectCRL = pvutils.getParametersValue(parameters, INDIRECT_CRL, IssuingDistributionPoint.defaultValues(INDIRECT_CRL));
    this.onlyContainsAttributeCerts = pvutils.getParametersValue(parameters, ONLY_CONTAINS_ATTRIBUTE_CERTS, IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_ATTRIBUTE_CERTS));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof DISTRIBUTION_POINT): DistributionPointName;
  public static override defaultValues(memberName: typeof ONLY_CONTAINS_USER_CERTS): boolean;
  public static override defaultValues(memberName: typeof ONLY_CONTAINS_CA_CERTS): boolean;
  public static override defaultValues(memberName: typeof ONLY_SOME_REASON): number;
  public static override defaultValues(memberName: typeof INDIRECT_CRL): boolean;
  public static override defaultValues(memberName: typeof ONLY_CONTAINS_ATTRIBUTE_CERTS): boolean;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case DISTRIBUTION_POINT:
        return [];
      case ONLY_CONTAINS_USER_CERTS:
        return false;
      case ONLY_CONTAINS_CA_CERTS:
        return false;
      case ONLY_SOME_REASON:
        return 0;
      case INDIRECT_CRL:
        return false;
      case ONLY_CONTAINS_ATTRIBUTE_CERTS:
        return false;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * IssuingDistributionPoint ::= SEQUENCE {
   *    distributionPoint          [0] DistributionPointName OPTIONAL,
   *    onlyContainsUserCerts      [1] BOOLEAN DEFAULT FALSE,
   *    onlyContainsCACerts        [2] BOOLEAN DEFAULT FALSE,
   *    onlySomeReasons            [3] ReasonFlags OPTIONAL,
   *    indirectCRL                [4] BOOLEAN DEFAULT FALSE,
   *    onlyContainsAttributeCerts [5] BOOLEAN DEFAULT FALSE }
   *
   * ReasonFlags ::= BIT STRING {
   *    unused                  (0),
   *    keyCompromise           (1),
   *    cACompromise            (2),
   *    affiliationChanged      (3),
   *    superseded              (4),
   *    cessationOfOperation    (5),
   *    certificateHold         (6),
   *    privilegeWithdrawn      (7),
   *    aACompromise            (8) }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    distributionPoint?: string;
    distributionPointNames?: string;
    onlyContainsUserCerts?: string;
    onlyContainsCACerts?: string;
    onlySomeReasons?: string;
    indirectCRL?: string;
    onlyContainsAttributeCerts?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Constructed({
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: [
            new asn1js.Choice({
              value: [
                new asn1js.Constructed({
                  name: (names.distributionPoint || EMPTY_STRING),
                  idBlock: {
                    tagClass: 3, // CONTEXT-SPECIFIC
                    tagNumber: 0 // [0]
                  },
                  value: [
                    new asn1js.Repeated({
                      name: (names.distributionPointNames || EMPTY_STRING),
                      value: GeneralName.schema()
                    })
                  ]
                }),
                new asn1js.Constructed({
                  name: (names.distributionPoint || EMPTY_STRING),
                  idBlock: {
                    tagClass: 3, // CONTEXT-SPECIFIC
                    tagNumber: 1 // [1]
                  },
                  value: RelativeDistinguishedNames.schema().valueBlock.value
                })
              ]
            })
          ]
        }),
        new asn1js.Primitive({
          name: (names.onlyContainsUserCerts || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          }
        }), // IMPLICIT boolean value
        new asn1js.Primitive({
          name: (names.onlyContainsCACerts || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          }
        }), // IMPLICIT boolean value
        new asn1js.Primitive({
          name: (names.onlySomeReasons || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 3 // [3]
          }
        }), // IMPLICIT BitString value
        new asn1js.Primitive({
          name: (names.indirectCRL || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 4 // [4]
          }
        }), // IMPLICIT boolean value
        new asn1js.Primitive({
          name: (names.onlyContainsAttributeCerts || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 5 // [5]
          }
        }) // IMPLICIT boolean value
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      IssuingDistributionPoint.schema({
        names: {
          distributionPoint: DISTRIBUTION_POINT,
          distributionPointNames: DISTRIBUTION_POINT_NAMES,
          onlyContainsUserCerts: ONLY_CONTAINS_USER_CERTS,
          onlyContainsCACerts: ONLY_CONTAINS_CA_CERTS,
          onlySomeReasons: ONLY_SOME_REASON,
          indirectCRL: INDIRECT_CRL,
          onlyContainsAttributeCerts: ONLY_CONTAINS_ATTRIBUTE_CERTS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    if (DISTRIBUTION_POINT in asn1.result) {
      switch (true) {
        case (asn1.result.distributionPoint.idBlock.tagNumber === 0): // GENERAL_NAMES variant
          this.distributionPoint = Array.from(asn1.result.distributionPointNames, element => new GeneralName({ schema: element }));
          break;
        case (asn1.result.distributionPoint.idBlock.tagNumber === 1): // RDN variant
          {
            this.distributionPoint = new RelativeDistinguishedNames({
              schema: new asn1js.Sequence({
                value: asn1.result.distributionPoint.valueBlock.value
              })
            });
          }
          break;
        default:
          throw new Error("Unknown tagNumber for distributionPoint: {$asn1.result.distributionPoint.idBlock.tagNumber}");
      }
    }

    if (ONLY_CONTAINS_USER_CERTS in asn1.result) {
      const view = new Uint8Array(asn1.result.onlyContainsUserCerts.valueBlock.valueHex);
      this.onlyContainsUserCerts = (view[0] !== 0x00);
    }

    if (ONLY_CONTAINS_CA_CERTS in asn1.result) {
      const view = new Uint8Array(asn1.result.onlyContainsCACerts.valueBlock.valueHex);
      this.onlyContainsCACerts = (view[0] !== 0x00);
    }

    if (ONLY_SOME_REASON in asn1.result) {
      const view = new Uint8Array(asn1.result.onlySomeReasons.valueBlock.valueHex);
      this.onlySomeReasons = view[0];
    }

    if (INDIRECT_CRL in asn1.result) {
      const view = new Uint8Array(asn1.result.indirectCRL.valueBlock.valueHex);
      this.indirectCRL = (view[0] !== 0x00);
    }

    if (ONLY_CONTAINS_ATTRIBUTE_CERTS in asn1.result) {
      const view = new Uint8Array(asn1.result.onlyContainsAttributeCerts.valueBlock.valueHex);
      this.onlyContainsAttributeCerts = (view[0] !== 0x00);
    }
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (this.distributionPoint) {
      let value;

      if (this.distributionPoint instanceof Array) {
        value = new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: Array.from(this.distributionPoint, o => o.toSchema())
        });
      } else {
        value = this.distributionPoint.toSchema();

        value.idBlock.tagClass = 3; // CONTEXT - SPECIFIC
        value.idBlock.tagNumber = 1; // [1]
      }

      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [value]
      }));
    }

    if (this.onlyContainsUserCerts !== IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_USER_CERTS)) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        valueHex: (new Uint8Array([0xFF])).buffer
      }));
    }

    if (this.onlyContainsCACerts !== IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_CA_CERTS)) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 2 // [2]
        },
        valueHex: (new Uint8Array([0xFF])).buffer
      }));
    }

    if (this.onlySomeReasons !== undefined) {
      const buffer = new ArrayBuffer(1);
      const view = new Uint8Array(buffer);

      view[0] = this.onlySomeReasons;

      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 3 // [3]
        },
        valueHex: buffer
      }));
    }

    if (this.indirectCRL !== IssuingDistributionPoint.defaultValues(INDIRECT_CRL)) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 4 // [4]
        },
        valueHex: (new Uint8Array([0xFF])).buffer
      }));
    }

    if (this.onlyContainsAttributeCerts !== IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_ATTRIBUTE_CERTS)) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 5 // [5]
        },
        valueHex: (new Uint8Array([0xFF])).buffer
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): IssuingDistributionPointJson {
    const obj: IssuingDistributionPointJson = {};

    if (this.distributionPoint) {
      if (this.distributionPoint instanceof Array) {
        obj.distributionPoint = Array.from(this.distributionPoint, o => o.toJSON());
      } else {
        obj.distributionPoint = this.distributionPoint.toJSON();
      }
    }

    if (this.onlyContainsUserCerts !== IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_USER_CERTS)) {
      obj.onlyContainsUserCerts = this.onlyContainsUserCerts;
    }

    if (this.onlyContainsCACerts !== IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_CA_CERTS)) {
      obj.onlyContainsCACerts = this.onlyContainsCACerts;
    }

    if (ONLY_SOME_REASON in this) {
      obj.onlySomeReasons = this.onlySomeReasons;
    }

    if (this.indirectCRL !== IssuingDistributionPoint.defaultValues(INDIRECT_CRL)) {
      obj.indirectCRL = this.indirectCRL;
    }

    if (this.onlyContainsAttributeCerts !== IssuingDistributionPoint.defaultValues(ONLY_CONTAINS_ATTRIBUTE_CERTS)) {
      obj.onlyContainsAttributeCerts = this.onlyContainsAttributeCerts;
    }

    return obj;
  }

}
