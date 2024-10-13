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
      image: `/images/designs/firefox-proton${useColorModeValue("-white", "-dark")}.jpg`,
    },
    {
      value: "lepton",
      title: t("design.lepton"),
      image: `/images/designs/firefox-lepton${useColorModeValue("-white", "-dark")}.jpg`,
    },
    {
      value: "photon",
      title: t("design.photon"),
      image: `/images/designs/firefox-photon${useColorModeValue("-white", "-dark")}.jpg`,
    },
    {
      value: "protonfix",
      title: t("design.protonfix"),
      image: `/images/designs/firefox-proton${useColorModeValue("-white", "-dark")}.jpg`,
    },
    {
      value: "fluerial",
      title: t("design.fluerial"),
      image: `/images/designs/floorp-fluerial${useColorModeValue("-white", "-dark")}.jpg`,
    },
  ];
};
