import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Input } from "@/components/common/input.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { InfoTip } from "@/components/common/infotip.tsx";
import { ExternalLink, Sliders } from "lucide-react";

export function BasicSettings() {
  const { t } = useTranslation();
  const { watch, setValue } = useFormContext();
  const watchAll = watch();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Sliders className="size-5" />
          {t("panelSidebar.basicSettings")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-3">
        <div>
          <div className="mb-2 inline-flex items-center gap-2">
            <h3 className="text-base font-medium">
              {t("panelSidebar.enableOrDisable")}
            </h3>
            <InfoTip
              description={t("panelSidebar.enableDescription")}
            />
          </div>
          <div className="flex items-center justify-between gap-2">
            <div className="space-y-1">
              <label
                htmlFor="enable-panel"
                className="text-sm font-medium leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70"
              >
                {t("panelSidebar.enablePanelSidebar")}
              </label>
            </div>
            <Switch
              id="enable-panel"
              checked={watchAll.enabled}
              onChange={(e) => setValue("enabled", e.currentTarget.checked)}
            />
          </div>
        </div>

        <div>
          <h3 className="text-base font-medium mb-2">
            {t("panelSidebar.otherSettings")}
          </h3>
          <div className="space-y-4">
            <div className="flex items-center justify-between gap-2">
              <label
                htmlFor="auto-unload"
                className="text-sm font-medium leading-none"
              >
                {t("panelSidebar.autoUnloadOnClose")}
              </label>
              <Switch
                id="auto-unload"
                checked={watchAll.autoUnload}
                onChange={(e) => setValue("autoUnload", e.target.checked)}
              />
            </div>

            <div className="space-y-2">
              <label className="text-sm font-medium leading-none">
                {t("panelSidebar.position")}
              </label>
              <div className="flex space-x-4">
                <label className="flex items-center space-x-2">
                  <input
                    type="radio"
                    name="position"
                    value="end"
                    checked={!watchAll.position_start}
                    onChange={() => setValue("position_start", false)}
                    className="radio"
                  />
                  <span>{t("panelSidebar.positionLeft")}</span>
                </label>
                <label className="flex items-center space-x-2">
                  <input
                    type="radio"
                    name="position"
                    value="start"
                    checked={watchAll.position_start}
                    onChange={() => setValue("position_start", true)}
                    className="radio"
                  />
                  <span>{t("panelSidebar.positionRight")}</span>
                </label>
              </div>
            </div>

            <div className="space-y-2">
              <label
                htmlFor="global-width"
                className="text-sm font-medium leading-none"
              >
                {t("panelSidebar.globalWidth")}
              </label>
              <Input
                id="global-width"
                type="number"
                value={watchAll.globalWidth || ""}
                onChange={(e) =>
                  setValue("globalWidth", Number(e.target.value))}
                className="w-full"
              />
              <p className="text-sm text-base-content/70">
                {t("panelSidebar.globalWidthDescription")}
              </p>
            </div>

            <div className="p-3 bg-base-200 rounded-lg">
              <p className="text-sm">
                {t("panelSidebar.iconProviderRemoved")}
              </p>
            </div>
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
