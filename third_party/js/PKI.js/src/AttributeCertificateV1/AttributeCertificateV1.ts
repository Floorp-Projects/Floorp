import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { AlgorithmIdentifier, AlgorithmIdentifierJson, AlgorithmIdentifierSchema } from "../AlgorithmIdentifier";
import { AttributeCertificateInfoV1, AttributeCertificateInfoV1Json, AttributeCertificateInfoV1Schema } from "./AttributeCertificateInfoV1";
import * as Schema from "../Schema";
import { PkiObject, PkiObjectParameters } from "../PkiObject";
import { AsnError } from "../errors";
import { EMPTY_STRING } from "../constants";

const ACINFO = "acinfo";
const SIGNATURE_ALGORITHM = "signatureAlgorithm";
const SIGNATURE_VALUE = "signatureValue";
const CLEAR_PROPS = [
  ACINFO,
  SIGNATURE_VALUE,
  SIGNATURE_ALGORITHM
];

export interface IAttributeCertificateV1 {
  /**
   * Attribute certificate information
   */
  acinfo: AttributeCertificateInfoV1;
  /**
   * Signature algorithm
   */
  signatureAlgorithm: AlgorithmIdentifier;
  /**
   * Signature value
   */
  signatureValue: asn1js.BitString;
}

export interface AttributeCertificateV1Json {
  acinfo: AttributeCertificateInfoV1Json;
  signatureAlgorithm: AlgorithmIdentifierJson;
  signatureValue: asn1js.BitStringJson;
}

export type AttributeCertificateV1Parameters = PkiObjectParameters & Partial<IAttributeCertificateV1>;

/**
 * Class from X.509:1997
 */
export class AttributeCertificateV1 extends PkiObject implements IAttributeCertificateV1 {

  public static override CLASS_NAME = "AttributeCertificateV1";

  public acinfo!: AttributeCertificateInfoV1;
  public signatureAlgorithm!: AlgorithmIdentifier;
  public signatureValue!: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link AttributeCertificateV1} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AttributeCertificateV1Parameters = {}) {
    super();

    this.acinfo = pvutils.getParametersValue(parameters, ACINFO, AttributeCertificateV1.defaultValues(ACINFO));
    this.signatureAlgorithm = pvutils.getParametersValue(parameters, SIGNATURE_ALGORITHM, AttributeCertificateV1.defaultValues(SIGNATURE_ALGORITHM));
    this.signatureValue = pvutils.getParametersValue(parameters, SIGNATURE_VALUE, AttributeCertificateV1.defaultValues(SIGNATURE_VALUE));

    if (parameters.schema) {
      this.fromSchema(parameters.schema);
    }
  }

  /**
   * Returns default values for all class members
   * @param memberName String name for a class member
   * @returns Default value
   */
  public static override defaultValues(memberName: typeof ACINFO): AttributeCertificateInfoV1;
  public static override defaultValues(memberName: typeof SIGNATURE_ALGORITHM): AlgorithmIdentifier;
  public static override defaultValues(memberName: typeof SIGNATURE_VALUE): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ACINFO:
        return new AttributeCertificateInfoV1();
      case SIGNATURE_ALGORITHM:
        return new AlgorithmIdentifier();
      case SIGNATURE_VALUE:
        return new asn1js.BitString();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AttributeCertificate ::= SEQUENCE {
   *   acinfo               AttributeCertificateInfoV1,
   *   signatureAlgorithm   AlgorithmIdentifier,
   *   signatureValue       BIT STRING
   * }
   *```
   */
  public static override schema(parameters: Schema.SchemaParameters<{
    acinfo?: AttributeCertificateInfoV1Schema;
    signatureAlgorithm?: AlgorithmIdentifierSchema;
    signatureValue?: string;
  }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        AttributeCertificateInfoV1.schema(names.acinfo || {}),
        AlgorithmIdentifier.schema(names.signatureAlgorithm || {}),
        new asn1js.BitString({ name: (names.signatureValue || EMPTY_STRING) })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    //#region Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      AttributeCertificateV1.schema({
        names: {
          acinfo: {
            names: {
              blockName: ACINFO
            }
          },
          signatureAlgorithm: {
            names: {
              blockName: SIGNATURE_ALGORITHM
            }
          },
          signatureValue: SIGNATURE_VALUE
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);
    //#endregion

    //#region Get internal properties from parsed schema
    this.acinfo = new AttributeCertificateInfoV1({ schema: asn1.result.acinfo });
    this.signatureAlgorithm = new AlgorithmIdentifier({ schema: asn1.result.signatureAlgorithm });
    this.signatureValue = asn1.result.signatureValue;
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    return (new asn1js.Sequence({
      value: [
        this.acinfo.toSchema(),
        this.signatureAlgorithm.toSchema(),
        this.signatureValue
      ]
    }));
  }

  public toJSON(): AttributeCertificateV1Json {
    return {
      acinfo: this.acinfo.toJSON(),
      signatureAlgorithm: this.signatureAlgorithm.toJSON(),
      signatureValue: this.signatureValue.toJSON(),
    };
  }

}
