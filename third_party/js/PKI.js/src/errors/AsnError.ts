export interface AsnFromBerResult {
  offset: number;
  result: any;
}

export interface AsnCompareSchemaResult {
  verified: boolean;
  result?: any;
}

export class AsnError extends Error {

  static assertSchema(asn1: AsnCompareSchemaResult, target: string): asserts asn1 is { verified: true, result: any; } {
    if (!asn1.verified) {
      throw new Error(`Object's schema was not verified against input data for ${target}`);
    }
  }

  public static assert(asn: AsnFromBerResult, target: string): void {
    if (asn.offset === -1) {
      throw new AsnError(`Error during parsing of ASN.1 data. Data is not correct for '${target}'.`);
    }
  }

  constructor(message: string) {
    super(message);

    this.name = "AsnError";
  }

}