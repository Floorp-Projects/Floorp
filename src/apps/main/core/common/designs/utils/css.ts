import { zFloorpDesignConfigs } from "../../../../../common/scripts/global-types/type";
import leptonTabStyles from "@nora/skin/lepton/css/leptonContent.css?url";
import leptonUserJs from "@nora/skin/lepton/userjs/lepton.js?raw";
import photonUserJs from "@nora/skin/lepton/userjs/photon.js?raw";
import protonfixUserJs from "@nora/skin/lepton/userjs/protonfix.js?raw";
import fluerialStyles from "@nora/skin/fluerial/css/fluerial.css?url";
import leptonChromeStyles from "@nora/skin/lepton/css/leptonChrome.css?url";
import { z } from "zod";

interface FCSS {
  styles: string[] | null;
  userjs: string | null;
}

export function getCSSFromConfig(pref: z.infer<typeof zFloorpDesignConfigs>): FCSS {
  switch (pref.globalConfigs.userInterface) {
    case "fluerial": {
      return { styles: [fluerialStyles], userjs: null };
    }
    case "lepton": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: leptonUserJs,
      };
    }
    case "photon": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: photonUserJs,
      };
    }
    case "protonfix": {
      return {
        styles: [leptonChromeStyles, leptonTabStyles],
        userjs: protonfixUserJs,
      };
    }
    case "proton": {
      return {
        styles: null,
        userjs: null,
      };
    }
    default: {
      pref.globalConfigs.userInterface satisfies never;
      return {
        styles:null,
        userjs:null
      }
    }
  }
}