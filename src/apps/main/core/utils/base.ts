import { ViteHotContext } from "vite/types/hot";
import { kebabCase } from 'es-toolkit/string';

import { consola, InputLogObject } from "consola/browser";

import type { ClassDecorator } from "./decorator";


// U+2063 before `@` needed
//https://github.com/microsoft/TypeScript/issues/47679


/**
 * @exmaple ```ts
 * ⁣@noraComponent(import.meta.hot)
 * class FooBar extends NoraComponentBase {}
 * ```
 */
export function noraComponent(aViteHotContext : ViteHotContext | undefined): ClassDecorator<NoraComponentBase> {
  return (clazz,ctx) => {
    aViteHotContext?.accept((module)=>{
      if (module && module.default) {
        new module.default();
      }
    });
    if (_NoraComponentBase_map_logger.has(ctx.name!)) {
      throw new Error(`Duplicate NoraComponent Name: ${ctx.name}`);
    }

    const _console = console.createInstance({
      "prefix":`nora@${kebabCase(ctx.name!)}`,
      
    });
    _NoraComponentBase_map_logger.set(ctx.name!,_console)
    console.debug("[nora@base] noraComponent");
  }
}

let _NoraComponentBase_map_logger = new Map<string,ConsoleInstance>()
export class NoraComponentBase {
  get logger() {
    const _logger = _NoraComponentBase_map_logger.get(this.constructor.name)
    if (_logger) {
      return _logger;
    } else {
      throw new Error("logger is not implemented")
    }
  }
}