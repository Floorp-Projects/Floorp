import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { DigestInfo, DigestInfoJson, DigestInfoSchema } from "./DigestInfo";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const MAC = "mac";
const MAC_SALT = "macSalt";
const ITERATIONS = "iterations";
const CLEAR_PROPS = [
  MAC,
  MAC_SALT,
  ITERATIONS
];

export interface IMacData {
  mac: DigestInfo;
  macSalt: asn1js.OctetString;
  iterations?: number;
}

export interface MacDataJson {
  mac: DigestInfoJson;
  macSalt: asn1js.OctetStringJson;
  iterations?: number;
}

export type MacDataParameters = PkiObjectParameters & Partial<IMacData>;

export type MacDataSchema = Schema.SchemaParameters<{
  mac?: DigestInfoSchema;
  macSalt?: string;
  iterations?: string;
}>;

/**
 * Represents the MacData structure described in [RFC7292](https://datatracker.ietf.org/doc/html/rfc7292)
 */
export class MacData extends PkiObject implements IMacData {

  public static override CLASS_NAME = "MacData";

  public mac!: DigestInfo;
  public macSalt!: asn1js.OctetString;
  public iterations?: number;

  /**
   * Initializes a new instance of the {@link MacData} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: MacDataParameters = {}) {
    super();

    this.mac = pvutils.getParametersValue(parameters, MAC, MacData.defaultValues(MAC));
    this.macSalt = pvutils.getParametersValue(parameters, MAC_SALT, MacData.defaultValues(MAC_SALT));
    if (ITERATIONS in parameters) {
      this.iterations = pvutils.getParametersValue(parameters, ITERATIONS, MacData.defaultValues(ITERATIONS));
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
  public static override defaultValues(memberName: typeof MAC): DigestInfo;
  public static override defaultValues(memberName: typeof MAC_SALT): asn1js.OctetString;
  public static override defaultValues(memberName: typeof ITERATIONS): number;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case MAC:
        return new DigestInfo();
      case MAC_SALT:
        return new asn1js.OctetString();
      case ITERATIONS:
        return 1;
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
      case MAC:
        return ((DigestInfo.compareWithDefault("digestAlgorithm", memberValue.digestAlgorithm)) &&
          (DigestInfo.compareWithDefault("digest", memberValue.digest)));
      case MAC_SALT:
        return (memberValue.isEqual(MacData.defaultValues(memberName)));
      case ITERATIONS:
        return (memberValue === MacData.defaultValues(memberName));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * MacData ::= SEQUENCE {
   *    mac           DigestInfo,
   *    macSalt       OCTET STRING,
   *    iterations    INTEGER DEFAULT 1
   *    -- Note: The default is for historical reasons and its use is
   *    -- deprecated. A higher value, like 1024 is recommended.
   *    }
   *```
   */
  public static override schema(parameters: MacDataSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      optional: (names.optional || true),
      value: [
        DigestInfo.schema(names.mac || {
          names: {
            blockName: MAC
          }
        }),
        new asn1js.OctetString({ name: (names.macSalt || MAC_SALT) }),
        new asn1js.Integer({
          optional: true,
          name: (names.iterations || ITERATIONS)
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
      MacData.schema({
        names: {
          mac: {
            names: {
              blockName: MAC
            }
          },
          macSalt: MAC_SALT,
          iterations: ITERATIONS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.mac = new DigestInfo({ schema: asn1.result.mac });
    this.macSalt = asn1.result.macSalt;
    if (ITERATIONS in asn1.result)
      this.iterations = asn1.result.iterations.valueBlock.valueDec;
  }

  public toSchema(): asn1js.Sequence {
    //#region Construct and return new ASN.1 schema for this object
    const outputArray: any[] = [
      this.mac.toSchema(),
      this.macSalt
    ];

    if (this.iterations !== undefined) {
      outputArray.push(new asn1js.Integer({ value: this.iterations }));
    }

    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): MacDataJson {
    const res: MacDataJson = {
      mac: this.mac.toJSON(),
      macSalt: this.macSalt.toJSON(),
    };

    if (this.iterations !== undefined) {
      res.iterations = this.iterations;
    }

    return res;
  }

}
