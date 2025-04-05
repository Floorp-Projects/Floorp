import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { ExternalLink, Settings } from "lucide-react";
import { useState } from "react";
import { RestartModal } from "@/components/common/restart-modal.tsx";

export function Preferences() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();
  const [showRestartModal, setShowRestartModal] = useState(false);

  return (
    <>
      {showRestartModal ? (
        <RestartModal
          onClose={() => setShowRestartModal(false)}
          label={t("progressiveWebApp.needRestartDescriptionForEnableAndDisable")}
        />
      ) : null}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Settings className="size-5" />
            {t("progressiveWebApp.basicSettings")}
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          <div>
            <h3 className="text-base font-medium mb-2">
              {t("progressiveWebApp.enableDisable")}
            </h3>
            <div className="flex items-center justify-between gap-2">
              <div className="space-y-1">
                <label htmlFor="enable-pwa" className="font-medium">
                  {t("progressiveWebApp.enablePwa")}
                </label>
                <p className="text-sm text-base-content/70">
                  {t("progressiveWebApp.enablePwaDescription")}
                </p>
              </div>
              <Switch
                id="enable-pwa"
                checked={!!getValues("enabled")}
                onChange={(e) => {
                  setValue("enabled", e.target.checked);
                  setShowRestartModal(true);
                }}
              />
            </div>
          </div>

          <div>
            <h3 className="text-base font-medium mb-2">
              {t("progressiveWebApp.otherSettings")}
            </h3>
            <div className="flex items-center justify-between gap-2">
              <div className="space-y-1">
                <label htmlFor="show-toolbar" className="font-medium">
                  {t("progressiveWebApp.showToolbar")}
                </label>
                <p className="text-sm text-base-content/70">
                  {t("progressiveWebApp.showToolbarDescription")}
                </p>
              </div>
              <Switch
                id="show-toolbar"
                checked={!!getValues("showToolbar")}
                onChange={(e) => setValue("showToolbar", e.target.checked)}
              />
            </div>
          </div>

          <div>
            <a
              href="https://docs.floorp.app/docs/features/how-to-use-pwa"
              target="_blank"
              rel="noreferrer"
              className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
            >
              {t("progressiveWebApp.learnMore")}
              <ExternalLink className="size-4" />
            </a>
          </div>
        </CardContent>
      </Card>
    </>
  );
}
