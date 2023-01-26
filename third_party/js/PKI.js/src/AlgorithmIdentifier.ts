import * as asn1js from "asn1js";
import * as pvutils from "pvutils";
import { EMPTY_STRING } from "./constants";
import { AsnError } from "./errors";
import { PkiObject, PkiObjectParameters } from "./PkiObject";
import * as Schema from "./Schema";

const ALGORITHM_ID = "algorithmId";
const ALGORITHM_PARAMS = "algorithmParams";
const ALGORITHM = "algorithm";
const PARAMS = "params";
const CLEAR_PROPS = [
  ALGORITHM,
  PARAMS
];

export interface IAlgorithmIdentifier {
  /**
   * ObjectIdentifier for algorithm (string representation)
   */
  algorithmId: string;
  /**
   * Any algorithm parameters
   */
  algorithmParams?: any;
}

export type AlgorithmIdentifierParameters = PkiObjectParameters & Partial<IAlgorithmIdentifier>;

/**
 * JSON representation of {@link AlgorithmIdentifier}
 */
export interface AlgorithmIdentifierJson {
  algorithmId: string;
  algorithmParams?: any;
}

export type AlgorithmIdentifierSchema = Schema.SchemaParameters<{
  algorithmIdentifier?: string;
  algorithmParams?: string;
}>;

/**
 * Represents the AlgorithmIdentifier structure described in [RFC5280](https://datatracker.ietf.org/doc/html/rfc5280)
 */
export class AlgorithmIdentifier extends PkiObject implements IAlgorithmIdentifier {

  public static override CLASS_NAME = "AlgorithmIdentifier";

  public algorithmId!: string;
  public algorithmParams?: any;

  /**
   * Initializes a new instance of the {@link AlgorithmIdentifier} class
   * @param parameters Initialization parameters
   */
  constructor(parameters: AlgorithmIdentifierParameters = {}) {
    super();

    this.algorithmId = pvutils.getParametersValue(parameters, ALGORITHM_ID, AlgorithmIdentifier.defaultValues(ALGORITHM_ID));
    if (ALGORITHM_PARAMS in parameters) {
      this.algorithmParams = pvutils.getParametersValue(parameters, ALGORITHM_PARAMS, AlgorithmIdentifier.defaultValues(ALGORITHM_PARAMS));
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
  public static override defaultValues(memberName: typeof ALGORITHM_ID): string;
  public static override defaultValues(memberName: typeof ALGORITHM_PARAMS): any;
  public static override defaultValues(memberName: string): any {
    switch (memberName) {
      case ALGORITHM_ID:
        return EMPTY_STRING;
      case ALGORITHM_PARAMS:
        return new asn1js.Any();
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * Compares values with default values for all class members
   * @param memberName String name for a class member
   * @param memberValue Value to compare with default value
   */
  static compareWithDefault(memberName: string, memberValue: any): boolean {
    switch (memberName) {
      case ALGORITHM_ID:
        return (memberValue === EMPTY_STRING);
      case ALGORITHM_PARAMS:
        return (memberValue instanceof asn1js.Any);
      default:
        return super.defaultValues(memberName);
    }
  }

  /**
   * @inheritdoc
   * @asn ASN.1 schema
   * ```asn
   * AlgorithmIdentifier ::= Sequence  {
   *    algorithm               OBJECT IDENTIFIER,
   *    parameters              ANY DEFINED BY algorithm OPTIONAL  }
   *```
   */
  public static override schema(parameters: AlgorithmIdentifierSchema = {}): any {
    const names = pvutils.getParametersValue<NonNullable<typeof parameters.names>>(parameters, "names", {});

    return (new asn1js.Sequence({
      name: (names.blockName || EMPTY_STRING),
      optional: (names.optional || false),
      value: [
        new asn1js.ObjectIdentifier({ name: (names.algorithmIdentifier || EMPTY_STRING) }),
        new asn1js.Any({ name: (names.algorithmParams || EMPTY_STRING), optional: true })
      ]
    }));
  }

  public fromSchema(schema: Schema.SchemaType): void {
    // Clear input data first
    pvutils.clearProps(schema, CLEAR_PROPS);

    // Check the schema is valid
    const asn1 = asn1js.compareSchema(schema,
      schema,
      AlgorithmIdentifier.schema({
        names: {
          algorithmIdentifier: ALGORITHM,
          algorithmParams: PARAMS
        }
      })
    );
    AsnError.assertSchema(asn1, this.className);

    // Get internal properties from parsed schema
    this.algorithmId = asn1.result.algorithm.valueBlock.toString();
    if (PARAMS in asn1.result) {
      this.algorithmParams = asn1.result.params;
    }
  }

  public toSchema(): asn1js.Sequence {
    // Create array for output sequence
    const outputArray = [];
    outputArray.push(new asn1js.ObjectIdentifier({ value: this.algorithmId }));
    if (this.algorithmParams && !(this.algorithmParams instanceof asn1js.Any)) {
      outputArray.push(this.algorithmParams);
    }

    // Construct and return new ASN.1 schema for this object
    return (new asn1js.Sequence({
      value: outputArray
    }));
  }

  public toJSON(): AlgorithmIdentifierJson {
    const object: AlgorithmIdentifierJson = {
      algorithmId: this.algorithmId
    };

    if (this.algorithmParams && !(this.algorithmParams instanceof asn1js.Any)) {
      object.algorithmParams = this.algorithmParams.toJSON();
    }

    return object;
  }

  /**
   * Checks that two "AlgorithmIdentifiers" are equal
   * @param algorithmIdentifier
   */
  public isEqual(algorithmIdentifier: unknown): boolean {
    //#region Check input type
    if (!(algorithmIdentifier instanceof AlgorithmIdentifier)) {
      return false;
    }
    //#endregion

    //#region Check "algorithm_id"
    if (this.algorithmId !== algorithmIdentifier.algorithmId) {
      return false;
    }
    //#endregion

    //#region Check "algorithm_params"
    if (this.algorithmParams) {
      if (algorithmIdentifier.algorithmParams) {
        return JSON.stringify(this.algorithmParams) === JSON.stringify(algorithmIdentifier.algorithmParams);
      }

      return false;
    }

    if (algorithmIdentifier.algorithmParams) {
      return false;
    }
    //#endregion

    return true;
  }

}
