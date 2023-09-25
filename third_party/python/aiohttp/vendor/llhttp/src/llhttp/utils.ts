export interface IEnumMap {
  [key: string]: number;
}

export function enumToMap(
  obj: any,
  filter?: ReadonlyArray<number>,
  exceptions?: ReadonlyArray<number>,
): IEnumMap {
  const res: IEnumMap = {};

  for (const key of Object.keys(obj)) {
    const value = obj[key];
    if (typeof value !== 'number') {
      continue;
    }
    if (filter && !filter.includes(value)) {
      continue;
    }
    if (exceptions && exceptions.includes(value)) {
      continue;
    }
    res[key] = value;
  }

  return res;
}
