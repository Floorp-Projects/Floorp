// biome-ignore lint/complexity/noBannedTypes: <explanation>
export interface Type<T> extends Function {
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  new (...args: any[]): T;
}

type MixinArgument =
  | {
      path: string;
      type: "class";
      export: boolean;
      extends: string;
      className: string;
    }
  | {
      path: string;
      type: "function";
      export: boolean;
      funcName: string;
    };

const map: Map<MixinArgument, Array<object>> = new Map();

export function Mixin(value: MixinArgument) {
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  return (_constructor: Type<any>) => {
    const tmp = class extends _constructor {
      __meta__ = {
        meta: value,
        inject: Object.getOwnPropertyNames(_constructor.prototype)
          .filter((v) => v !== "constructor")
          .map((v) => {
            this[v]();
            const tmp = this[`__meta_${v}__`];
            delete this[`__meta_${v}__`];
            return tmp;
          }),
      };
    };

    return tmp;
  };
}

type InjectMetadata = {
  /**
   * only in type `class`, `method` used
   */
  method?: string;
  at: { value: "INVOKE"; target: string };
};

export function Inject(value: InjectMetadata) {
  // biome-ignore lint/suspicious/noExplicitAny: <explanation>
  return (target: any, context: ClassMethodDecoratorContext) => {
    return function (this, ...args) {
      this[`__meta_${context.name.toString()}__`] = {
        meta: value,
        func: target.toString(),
      };
      return;
    };
  };
}
