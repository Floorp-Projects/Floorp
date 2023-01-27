export interface ECNamedCurve {
  /**
   * The curve ASN.1 object identifier
   */
  id: string;
  /**
   * The name of the curve
   */
  name: string;
  /**
   * The coordinate length in bytes
   */
  size: number;
}

export class ECNamedCurves {

  public static readonly namedCurves: Record<string, ECNamedCurve> = {};

  /**
   * Registers an ECC named curve
   * @param name The name o the curve
   * @param id The curve ASN.1 object identifier
   * @param size The coordinate length in bytes
   */
  public static register(name: string, id: string, size: number): void {
    this.namedCurves[name.toLowerCase()] = this.namedCurves[id] = { name, id, size };
  }

  /**
  * Returns an ECC named curve object
  * @param nameOrId Name or identifier of the named curve
  * @returns
  */
  static find(nameOrId: string): ECNamedCurve | null {
    return this.namedCurves[nameOrId.toLowerCase()] || null;
  }

  static {
    // Register default curves

    // NIST
    this.register("P-256", "1.2.840.10045.3.1.7", 32);
    this.register("P-384", "1.3.132.0.34", 48);
    this.register("P-521", "1.3.132.0.35", 66);

    // Brainpool
    this.register("brainpoolP256r1", "1.3.36.3.3.2.8.1.1.7", 32);
    this.register("brainpoolP384r1", "1.3.36.3.3.2.8.1.1.11", 48);
    this.register("brainpoolP512r1", "1.3.36.3.3.2.8.1.1.13", 64);
  }

}
