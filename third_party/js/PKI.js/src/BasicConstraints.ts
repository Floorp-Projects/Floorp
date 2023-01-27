import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const PATH_LENGTH_CONSTRAINT = "pathLenConstraint";
const CA = "cA";

export interface IBasicConstraints {
  cA: boolean;
  pathLenConstraint?: number | asn1js.Integer;
}

export type BasicConstraintsParameters = PkiObjectParameters & Partial<IBasicConstraints>;

export interface BasicConstraintsJson {
  cA?: boolean;
  pathLenConstraint?: asn1js.IntegerJson | number;
}

/**
 * Represents the BasicConstraints structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class BasicConstraints extends PkiObject implements IBasicConstraints {

  public static override CLASS_NAME = "BasicConstraints";

  public cA!: boolean;
  public pathLenConstraint?: number | asn1js.Integer;

  /**
   * Initializes a new instance of the {@link AuthorityKeyIdentifier} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: BasicConstraintsParameters = {}) {
    super();

    this.cA = pvutils.getParametersValue(parameters, CA, false);
    if (PATH_LENGTH_CONSTRAINT in parameters) {
      this.pathLenConstraint = pvutils.getParametersValue(parameters, PATH_LENGTH_CONSTRAINT, 0);
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
  public static override defaultValues(memberName: typeof CA): boolean;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case CA:
        return false;
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * BasicConstraints ::= SEQUENCE {
   *    cA                      BOOLEAN DEFAULT FALSE,
   *    pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
   *```
   */
  static override schema(parameters: Schema.SchemaParameters<{ cA?: string; pathLenConstraint?: string; }> = {}): Schema.SchemaType {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      value: [
        new asn1js.Boolean({
          optional: true,
          name: (names.cA || EMPTY_STRING)
        }),
        new asn1js.Integer({
          optional: true,
          name: (names.pathLenConstraint || EMPTY_STRING)
        })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, [
      CA,
      PATH_LENGTH_CONSTRAINT
    ]);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      BasicConstraints.schema({
        names: {
          cA: CA,
          pathLenConstraint: PATH_LENGTH_CONSTRAINT
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    //#region Get internal properties from parsed schema
    if (CA in asn1.result) {
      this.cA = asn1.result.cA.valueBlock.value;
    }

    if (PATH_LENGTH_CONSTRAINT in asn1.result) {
      if (asn1.result.pathLenConstraint.valueBlock.isHexOnly) {
        this.pathLenConstraint = asn1.result.pathLenConstraint;
      } else {
        this.pathLenConstraint = asn1.result.pathLenConstraint.valueBlock.valueDec;
      }
    }
    //#endregion
  }

  public toSchema(): asn1js.Sequence {
    //#region Create array for output sequence
    const outputArray = [];

    if (this.cA !== BasicConstraints.defaultValues(CA))
      outputArray.push(new asn1js.Boolean({ value: this.cA }));

    if (PATH_LENGTH_CONSTRAINT in this) {
      if (this.pathLenConstraint instanceof asn1js.Integer) {
        outputArray.push(this.pathLenConstraint);
      } else {
        outputArray.push(new asn1js.Integer({ value: this.pathLenConstraint }));
      }
    }
    //#endregion

    //#region Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
    //#endregion
  }

  /**
   * Conversion for the class to JSON object
   * @returns
   */
  public toJSON(): BasicConstraintsJson {
    const object: BasicConstraintsJson = {};

    if (this.cA !== BasicConstraints.defaultValues(CA)) {
      object.cA = this.cA;
    }

    if (PATH_LENGTH_CONSTRAINT in this) {
      if (this.pathLenConstraint instanceof asn1js.Integer) {
        object.pathLenConstraint = this.pathLenConstraint.toJSON();
      } else {
        object.pathLenConstraint = this.pathLenConstraint;
      }
    }

    return object;
  }

}

