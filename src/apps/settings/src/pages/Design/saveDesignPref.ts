import {
  type DesignFormData,
  cZFloorpDesignConfigs,
  zFloorpDesignConfigsType,
} from "../../type";
import { setBoolPref, setIntPref, setStringPref } from "../../dev";

export function saveDesignSettings(settings: DesignFormData) {
  const parsedSettings = cZFloorpDesignConfigs.parse(settings);
  console.log(settings);
}
