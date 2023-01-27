import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const STATUS = "status";
const STATUS_STRINGS = "statusStrings";
const FAIL_INFO = "failInfo";
const CLEAR_PROPS = [
  STATUS,
  STATUS_STRINGS,
  FAIL_INFO
];

export interface IPKIStatusInfo {
  status: PKIStatus;
  statusStrings?: asn1js.Utf8String[];
  failInfo?: asn1js.BitString;
}

export interface PKIStatusInfoJson {
  status: PKIStatus;
  statusStrings?: asn1js.Utf8StringJson[];
  failInfo?: asn1js.BitStringJson;
}

export type PKIStatusInfoParameters = PkiObjectParameters & Partial<IPKIStatusInfo>;

export type PKIStatusInfoSchema = Schema.SchemaParameters<{
  status?: string;
  statusStrings?: string;
  failInfo?: string;
}>;

export enum PKIStatus {
  granted = 0,
  grantedWithMods = 1,
  rejection = 2,
  waiting = 3,
  revocationWarning = 4,
  revocationNotification = 5,
}

/**
 * Represents the PKIStatusInfo structure described in [RFC3161](https://www.ietf.org/rfc/rfc3161.txt)
 */
export class PKIStatusInfo extends PkiObject implements IPKIStatusInfo {

  public static override CLASS_NAME = "PKIStatusInfo";

  public status!: PKIStatus;
  public statusStrings?: asn1js.Utf8String[];
  public failInfo?: asn1js.BitString;

  /**
   * Initializes a new instance of the {@link PBKDF2Params} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: PKIStatusInfoParameters = {}) {
    super();

    this.status = pvutils.getParametersValue(parameters, STATUS, PKIStatusInfo.defaultValues(STATUS));
    if (STATUS_STRINGS in parameters) {
      this.statusStrings = pvutils.getParametersValue(parameters, STATUS_STRINGS, PKIStatusInfo.defaultValues(STATUS_STRINGS));
    }
    if (FAIL_INFO in parameters) {
      this.failInfo = pvutils.getParametersValue(parameters, FAIL_INFO, PKIStatusInfo.defaultValues(FAIL_INFO));
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
  public static override defaultValues(memberName: typeof STATUS): number;
  public static override defaultValues(memberName: typeof STATUS_STRINGS): asn1js.Utf8String[];
  public static override defaultValues(memberName: typeof FAIL_INFO): asn1js.BitString;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case STATUS:
        return 2;
      case STATUS_STRINGS:
        return [];
      case FAIL_INFO:
        return new asn1js.BitString();
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
      case STATUS:
        return (memberValue === PKIStatusInfo.defaultValues(memberName));
      case STATUS_STRINGS:
        return (memberValue.length === 0);
      case FAIL_INFO:
        return (memberValue.isEqual(PKIStatusInfo.defaultValues(memberName)));
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * PKIStatusInfo ::= SEQUENCE {
   *    status        PKIStatus,
   *    statusString  PKIFreeText     OPTIONAL,
   *    failInfo      PKIFailureInfo  OPTIONAL  }
   *```
   */
  public static override schema(parameters: PKIStatusInfoSchema = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Integer({ name: (names.status || EMPTY_STRING) }),
        new asn1js.Sequence({
          optional: true,
          value: [
            new asn1js.Repeated({
              name: (names.statusStrings || EMPTY_STRING),
              value: new asn1js.Utf8String()
            })
          ]
        }),
        new asn1js.BitString({
          name: (names.failInfo || EMPTY_STRING),
          optional: true
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
      PKIStatusInfo.schema({
        names: {
          status: STATUS,
          statusStrings: STATUS_STRINGS,
          failInfo: FAIL_INFO
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    const _status = asn1.result.status;

    if ((_status.valueBlock.isHexOnly === true) ||
      (_status.valueBlock.valueDec < 0) ||
      (_status.valueBlock.valueDec > 5))
      throw new Error("PKIStatusInfo \"status\" has invalid value");

    this.status = _status.valueBlock.valueDec;

    if (STATUS_STRINGS in asn1.result)
      this.statusStrings = asn1.result.statusStrings;
    if (FAIL_INFO in asn1.result)
      this.failInfo = asn1.result.failInfo;
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array of output sequence
    const outputArray = [];

    outputArray.push(new asn1js.Integer({ value: this.status }));

    if (this.statusStrings) {
      outputArray.push(new asn1js.Sequence({
        optional: true,
        value: this.statusStrings
      }));
    }

    if (this.failInfo) {
      outputArray.push(this.failInfo);
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  public toJSON(): PKIStatusInfoJson {
    const res: PKIStatusInfoJson = {
      status: this.status
    };

    if (this.statusStrings) {
      res.statusStrings = Array.from(this.statusStrings, o => o.toJSON());
    }
    if (this.failInfo) {
      res.failInfo = this.failInfo.toJSON();
    }

    return res;
  }

}
