import * as constants from './constants';
import { enumToMap, IEnumMap } from './utils';

type Encoding = 'none' | 'hex';

export class CHeaders {
  public build(): string {
    let res = '';

    res += '#ifndef LLLLHTTP_C_HEADERS_\n';
    res += '#define LLLLHTTP_C_HEADERS_\n';

    res += '#ifdef __cplusplus\n';
    res += 'extern "C" {\n';
    res += '#endif\n';

    res += '\n';

    const errorMap = enumToMap(constants.ERROR);
    const methodMap = enumToMap(constants.METHODS);
    const httpMethodMap = enumToMap(constants.METHODS, constants.METHODS_HTTP, [
      constants.METHODS.PRI,
    ]);
    const rtspMethodMap = enumToMap(constants.METHODS, constants.METHODS_RTSP);
    const statusMap = enumToMap(constants.STATUSES, constants.STATUSES_HTTP);

    res += this.buildEnum('llhttp_errno', 'HPE', errorMap);
    res += '\n';
    res += this.buildEnum('llhttp_flags', 'F', enumToMap(constants.FLAGS),
      'hex');
    res += '\n';
    res += this.buildEnum('llhttp_lenient_flags', 'LENIENT',
      enumToMap(constants.LENIENT_FLAGS), 'hex');
    res += '\n';
    res += this.buildEnum('llhttp_type', 'HTTP',
      enumToMap(constants.TYPE));
    res += '\n';
    res += this.buildEnum('llhttp_finish', 'HTTP_FINISH',
      enumToMap(constants.FINISH));
    res += '\n';
    res += this.buildEnum('llhttp_method', 'HTTP', methodMap);
    res += '\n';
    res += this.buildEnum('llhttp_status', 'HTTP_STATUS', statusMap);

    res += '\n';

    res += this.buildMap('HTTP_ERRNO', errorMap);
    res += '\n';
    res += this.buildMap('HTTP_METHOD', httpMethodMap);
    res += '\n';
    res += this.buildMap('RTSP_METHOD', rtspMethodMap);
    res += '\n';
    res += this.buildMap('HTTP_ALL_METHOD', methodMap);
    res += '\n';
    res += this.buildMap('HTTP_STATUS', statusMap);

    res += '\n';

    res += '#ifdef __cplusplus\n';
    res += '}  /* extern "C" */\n';
    res += '#endif\n';
    res += '#endif  /* LLLLHTTP_C_HEADERS_ */\n';

    return res;
  }

  private buildEnum(name: string, prefix: string, map: IEnumMap,
                    encoding: Encoding = 'none'): string {
    let res = '';

    res += `enum ${name} {\n`;
    const keys = Object.keys(map);
    const keysLength = keys.length;
    for (let i = 0; i < keysLength; i++) {
      const key = keys[i];
      const isLast = i === keysLength - 1;

      let value: number | string = map[key];

      if (encoding === 'hex') {
        value = `0x${value.toString(16)}`;
      }

      res += `  ${prefix}_${key.replace(/-/g, '')} = ${value}`;
      if (!isLast) {
        res += ',\n';
      }
    }
    res += '\n};\n';
    res += `typedef enum ${name} ${name}_t;\n`;

    return res;
  }

  private buildMap(name: string, map: IEnumMap): string {
    let res = '';

    res += `#define ${name}_MAP(XX) \\\n`;
    for (const [key, value] of Object.entries(map)) {
      res += `  XX(${value!}, ${key.replace(/-/g, '')}, ${key}) \\\n`;
    }
    res += '\n';

    return res;
  }
}
