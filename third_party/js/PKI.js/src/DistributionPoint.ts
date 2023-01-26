import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { GeneralName, GeneralNameJson } from "./GeneralName";
import { DistributionPointName, DistributionPointNameJson } from "./IssuingDistributionPoint";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import { RelativeDistinguishedNames } from "./RelativeDistinguishedNames";
import * as Schema from "./Schema";

const DISTRIBUTION_POINT = "distributionPoint";
const DISTRIBUTION_POINT_NAMES = "distributionPointNames";
const REASONS = "reasons";
const CRL_ISSUER = "cRLIssuer";
const CRL_ISSUER_NAMES = "cRLIssuerNames";
const CLEAR_PROPS = [
  DISTRIBUTION_POINT,
  DISTRIBUTION_POINT_NAMES,
  REASONS,
  CRL_ISSUER,
  CRL_ISSUER_NAMES,
];

export interface IDistributionPoint {
  distributionPoint?: DistributionPointName;
  reasons?: asn1js.BitString;
  cRLIssuer?: GeneralName[];
}

export interface DistributionPointJson {
  distributionPoint?: DistributionPointNameJson;
  reasons?: asn1js.BitStringJson;
  cRLIssuer?: GeneralNameJson[];
}

export type DistributionPointParameters = PkiObjectParameters & Partial<IDistributionPoint>;

/**
 * Represents the DistributionPoint structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class DistributionPoint extends PkiObject implements IDistributionPoint {

  public static override CLASS_NAME = "DistributionPoint";

  public distributionPoint?: DistributionPointName;
  public reasons?: asn1js.BitString;
  public cRLIssuer?: GeneralName[];

  /**
   * Initializes a new instance of the {@link DistributionPoint} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: DistributionPointParameters = {}) {
    super();

    if (DISTRIBUTION_POINT in parameters) {
      this.distributionPoint = pvutils.getParametersValue(parameters, DISTRIBUTION_POINT, DistributionPoint.defaultValues(DISTRIBUTION_POINT));
    }
    if (REASONS in parameters) {
      this.reasons = pvutils.getParametersValue(parameters, REASONS, DistributionPoint.defaultValues(REASONS));
    }
    if (CRL_ISSUER in parameters) {
      this.cRLIssuer = pvutils.getParametersValue(parameters, CRL_ISSUER, DistributionPoint.defaultValues(CRL_ISSUER));
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
  public static override defaultValues(memberName: typeof DISTRIBUTION_POINT): DistributionPointName;
  public static override defaultValues(memberName: typeof REASONS): asn1js.BitString;
  public static override defaultValues(memberName: typeof CRL_ISSUER): GeneralName[];
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case DISTRIBUTION_POINT:
        return [];
      case REASONS:
        return new asn1js.BitString();
      case CRL_ISSUER:
        return [];
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * DistributionPoint ::= SEQUENCE {
   *    distributionPoint       [0]     DistributionPointName OPTIONAL,
   *    reasons                 [1]     ReasonFlags OPTIONAL,
   *    cRLIssuer               [2]     GeneralNames OPTIONAL }
   *
   * DistributionPointName ::= CHOICE {
   *    fullName                [0]     GeneralNames,
   *    nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
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
    reasons?: string;
    cRLIssuer?: string;
    cRLIssuerNames?: string;
  }> = {}): Schema.SchemaType {
    /**
     * @type {Object}
     * @property {string} [blockName]
     * @property {string} [distributionPoint]
     * @property {string} [distributionPointNames]
     * @property {string} [reasons]
     * @property {string} [cRLIssuer]
     * @property {string} [cRLIssuerNames]
     */
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
                  optional: true,
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
                  optional: true,
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
          name: (names.reasons || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          }
        }), // IMPLICIT BitString value
        new asn1js.Constructed({
          name: (names.cRLIssuer || EMPTY_STRING),
          optional: true,
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 2 // [2]
          },
          value: [
            new asn1js.Repeated({
              name: (names.cRLIssuerNames || EMPTY_STRING),
              value: GeneralName.schema()
            })
          ]
        }) // IMPLICIT BitString value
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      DistributionPoint.schema({
        names: {
          distributionPoint: DISTRIBUTION_POINT,
          distributionPointNames: DISTRIBUTION_POINT_NAMES,
          reasons: REASONS,
          cRLIssuer: CRL_ISSUER,
          cRLIssuerNames: CRL_ISSUER_NAMES
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    if (DISTRIBUTION_POINT in asn1.result) {
      if (asn1.result.distributionPoint.idBlock.tagNumber === 0) { // GENERAL_NAMES variant
        this.distributionPoint = Array.from(asn1.result.distributionPointNames, element => new GeneralName({ schema: element }));
      }

      if (asn1.result.distributionPoint.idBlock.tagNumber === 1) {// RDN variant
        this.distributionPoint = new RelativeDistinguishedNames({
          schema: new asn1js.Sequence({
            value: asn1.result.distributionPoint.valueBlock.value
          })
        });
      }
    }

    if (REASONS in asn1.result) {
      this.reasons = new asn1js.BitString({ valueHex: asn1.result.reasons.valueBlock.valueHex });
    }

    if (CRL_ISSUER in asn1.result) {
      this.cRLIssuer = Array.from(asn1.result.cRLIssuerNames, element => new GeneralName({ schema: element }));
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (this.distributionPoint) {
      let internalValue;

      if (this.distributionPoint instanceof Array) {
        internalValue = new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 0 // [0]
          },
          value: Array.from(this.distributionPoint, o => o.toSchema())
        });
      } else {
        internalValue = new asn1js.Constructed({
          idBlock: {
            tagClass: 3, // CONTEXT-SPECIFIC
            tagNumber: 1 // [1]
          },
          value: [this.distributionPoint.toSchema()]
        });
      }

      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 0 // [0]
        },
        value: [internalValue]
      }));
    }

    if (this.reasons) {
      outputArray.push(new asn1js.Primitive({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 1 // [1]
        },
        valueHex: this.reasons.valueBlock.valueHexView
      }));
    }

    if (this.cRLIssuer) {
      outputArray.push(new asn1js.Constructed({
        idBlock: {
          tagClass: 3, // CONTEXT-SPECIFIC
          tagNumber: 2 // [2]
        },
        value: Array.from(this.cRLIssuer, o => o.toSchema())
      }));
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): DistributionPointJson {
    const object: DistributionPointJson = {};

    if (this.distributionPoint) {
      if (this.distributionPoint instanceof Array) {
        object.distributionPoint = Array.from(this.distributionPoint, o => o.toJSON());
      } else {
        object.distributionPoint = this.distributionPoint.toJSON();
      }
    }

    if (this.reasons) {
      object.reasons = this.reasons.toJSON();
    }

    if (this.cRLIssuer) {
      object.cRLIssuer = Array.from(this.cRLIssuer, o => o.toJSON());
    }

    return object;
  }

}
