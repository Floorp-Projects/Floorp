import { useMemo } from 'react';
import { useTheme } from '@/components/theme-provider.tsx';
import { useTranslation } from 'react-i18next';

export const useInterfaceDesigns = () => {
  const { theme } = useTheme();
  const { t } = useTranslation();

  return useMemo(() => {
    const colorMode = theme === "dark" ? "Dark" : "Light";

    return [
      {
        value: "proton",
        title: t("design.proton"),
        image: `/images/designs/Floorp_UI_Proton_${colorMode}.svg`,
      },
      {
        value: "lepton",
        title: t("design.lepton"),
        image: `/images/designs/Floorp_UI_Lepton_${colorMode}.svg`,
      },
      {
        value: "photon",
        title: t("design.photon"),
        image: `/images/designs/Floorp_UI_Photon_${colorMode}.svg`,
      },
      {
        value: "protonfix",
        title: t("design.protonfix"),
        image: `/images/designs/Floorp_UI_ProtonFix_${colorMode}.svg`,
      },
      {
        value: "fluerial",
        title: t("design.fluerial"),
        image: `/images/designs/Floorp_UI_Fluerial_${colorMode}.svg`,
      },
    ];
  }, [theme, t]);
};
