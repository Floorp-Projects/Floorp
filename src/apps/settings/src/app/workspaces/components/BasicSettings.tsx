import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { InfoTip } from "@/components/common/infotip.tsx";
import { ExternalLink, Settings } from "lucide-react";

export function BasicSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Settings className="size-5" />
          {t("workspaces.basicSettings")}
        </CardTitle>
        <CardDescription>
          <a
            href="https://docs.floorp.app/docs/features/how-to-use-workspaces"
            className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
          >
            {t("workspaces.howToUseAndCustomize")}
            <ExternalLink className="size-4" />
          </a>
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-3">
        <div>
          <div className="mb-2 inline-flex items-center gap-2">
            <h3 className="text-base font-medium">
              {t("workspaces.enableOrDisable")}
            </h3>
            <InfoTip
              description={t("workspaces.enableWorkspacesDescription")}
            />
          </div>
          <div className="space-y-1">
            <div className="flex items-center justify-between gap-2">
              <div className="space-y-1">
                <label htmlFor="enable-workspaces">
                  {t("workspaces.enableWorkspaces")}
                </label>
              </div>
              <Switch
                id="enable-workspaces"
                checked={getValues("enabled")}
                onChange={(e) => setValue("enabled", e.target.checked)}
              />
            </div>
            <div className="text-sm text-base-content/70">
              {t("workspaces.needRestartDescriptionForEnableAndDisable")}
            </div>
          </div>
        </div>

        <h3 className="text-base font-medium">
          {t("workspaces.otherSettings")}
        </h3>
        <div className="space-y-3">
          <div className="flex items-center justify-between gap-2">
            <label htmlFor="close-popup" className="flex flex-col gap-1.5">
              <span>{t("workspaces.closePopupWhenSelectingWorkspace")}</span>
              <span className="font-normal text-sm text-base-content/70">
                {t("workspaces.closePopupWhenSelectingWorkspaceDescription")}
              </span>
            </label>
            <Switch
              id="close-popup"
              checked={getValues("closePopupAfterClick")}
              onChange={(e) =>
                setValue("closePopupAfterClick", e.target.checked)}
            />
          </div>

          <div className="flex items-center justify-between gap-2">
            <label htmlFor="show-name">
              {t("workspaces.showWorkspaceNameOnToolbar")}
            </label>
            <Switch
              id="show-name"
              checked={getValues("showWorkspaceNameOnToolbar")}
              onChange={(e) =>
                setValue("showWorkspaceNameOnToolbar", e.target.checked)}
            />
          </div>

          {/* <div className="flex items-center justify-between gap-2">
            <label htmlFor="manage-bms" className="flex flex-col gap-1.5">
              <span>{t("workspaces.manageOnBms")}</span>
              <span className="font-normal text-sm text-base-content/70">
                {t("workspaces.manageOnBmsDescription")}
              </span>
            </label>
            <Switch
              id="manage-bms"
              checked={getValues("manageOnBms")}
              onChange={(e) => setValue("manageOnBms", e.target.checked)}
            />
          </div> */}
        </div>
      </CardContent>
    </Card>
  );
}
