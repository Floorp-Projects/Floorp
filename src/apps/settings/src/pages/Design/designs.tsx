import { useColorModeValue } from "@chakra-ui/react";
import type { zFloorpDesignConfigsType } from "../../../../common/scripts/global-types/type";
import { useTranslation } from "react-i18next";

export const useInterfaceDesigns = (): {
  value: zFloorpDesignConfigsType["globalConfigs"]["userInterface"];
  title: string;
  image: string;
}[] => {
  const { t } = useTranslation();
  return [
    {
      value: "proton",
      title: t("design.proton"),
      image: `/images/designs/Floorp_UI_Proton_${useColorModeValue("Light", "Dark")}.svg`,
    },
    {
      value: "lepton",
      title: t("design.lepton"),
      image: `/images/designs/Floorp_UI_Lepton_${useColorModeValue("Light", "Dark")}.svg`,
    },
    {
      value: "photon",
      title: t("design.photon"),
      image: `/images/designs/Floorp_UI_Photon_${useColorModeValue("Light", "Dark")}.svg`,
    },
    {
      value: "protonfix",
      title: t("design.protonfix"),
      image: `/images/designs/Floorp_UI_ProtonFix_${useColorModeValue("Light", "Dark")}.svg`,
    },
    {
      value: "fluerial",
      title: t("design.fluerial"),
      image: `/images/designs/Floorp_UI_Fluerial_${useColorModeValue("Light", "Dark")}.svg`,
    },
  ];
};
