
export interface AnyConstructor {
  new(args: any): any;
}

export type ArgumentType =
  | "undefined"
  | "null"
  | "boolean"
  | "number"
  | "string"
  | "object"
  | "Array"
  | "ArrayBuffer"
  | "ArrayBufferView"
  | AnyConstructor;

export class ArgumentError extends TypeError {

  public static readonly NAME = "ArgumentError";

  public static isType(value: any, type: "undefined"): value is undefined;
  public static isType(value: any, type: "null"): value is null;
  public static isType(value: any, type: "boolean"): value is boolean;
  public static isType(value: any, type: "number"): value is number;
  public static isType(value: any, type: "object"): value is object;
  public static isType(value: any, type: "string"): value is string;
  public static isType(value: any, type: "Array"): value is any[];
  public static isType(value: any, type: "ArrayBuffer"): value is ArrayBuffer;
  public static isType(value: any, type: "ArrayBufferView"): value is ArrayBufferView;
  public static isType<T>(value: any, type: new (...args: any[]) => T): value is T;
  // @internal
  public static isType(value: any, type: ArgumentType): boolean;
  public static isType(value: any, type: ArgumentType): boolean {
    if (typeof type === "string") {
      if (type === "Array" && Array.isArray(value)) {
        return true;
      } else if (type === "ArrayBuffer" && value instanceof ArrayBuffer) {
        return true;
      } else if (type === "ArrayBufferView" && ArrayBuffer.isView(value)) {
        return true;
      } else if (typeof value === type) {
        return true;
      }
    } else if (value instanceof type) {
      return true;
    }

    return false;
  }

  public static assert(value: any, name: string, type: "undefined"): asserts value is undefined;
  public static assert(value: any, name: string, type: "null"): asserts value is null;
  public static assert(value: any, name: string, type: "boolean"): asserts value is boolean;
  public static assert(value: any, name: string, type: "number"): asserts value is number;
  public static assert(value: any, name: string, type: "object"): asserts value is { [key: string]: any; };
  public static assert(value: any, name: string, type: "string"): asserts value is string;
  public static assert(value: any, name: string, type: "Array"): asserts value is any[];
  public static assert(value: any, name: string, type: "ArrayBuffer"): asserts value is ArrayBuffer;
  public static assert(value: any, name: string, type: "ArrayBufferView"): asserts value is ArrayBufferView;
  public static assert<T>(value: any, name: string, type: new (...args: any[]) => T): asserts value is T;
  public static assert(value: any, name: string, type: ArgumentType, ...types: ArgumentType[]): void;
  public static assert(value: any, name: string, ...types: ArgumentType[]): void {
    for (const type of types) {
      if (this.isType(value, type)) {
        return;
      }
    }

    const typeNames = types.map(o => o instanceof Function && "name" in o ? o.name : `${o}`);
    throw new ArgumentError(`Parameter '${name}' is not of type ${typeNames.length > 1 ? `(${typeNames.join(" or ")})` : typeNames[0]}`);
  }

  public override name: typeof ArgumentError.NAME = ArgumentError.NAME;

}
