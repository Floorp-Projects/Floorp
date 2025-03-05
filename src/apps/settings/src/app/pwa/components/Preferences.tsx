import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";

export function Preferences() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();

  return (
    <Card>
      <CardHeader>
        <CardTitle>{t("progressiveWebApp.basicSettings")}</CardTitle>
      </CardHeader>
      <CardContent className="space-y-6">
        <div>
          <h3 className="text-base font-medium mb-2">
            {t("progressiveWebApp.enableDisable")}
          </h3>
          <div className="flex items-center justify-between">
            <div className="space-y-1">
              <label>{t("progressiveWebApp.enablePwa")}</label>
              <p className="text-sm text-base-content/70">
                {t("progressiveWebApp.enablePwaDescription")}
              </p>
            </div>
            <input
              type="checkbox"
              checked={!!getValues("enabled")}
              onChange={(e) => setValue("enabled", e.target.checked)}
              className="checkbox"
            />
          </div>
        </div>

        <div className="border-t pt-6">
          <h3 className="text-base font-medium mb-2">
            {t("progressiveWebApp.otherSettings")}
          </h3>
          <div className="flex items-center justify-between">
            <div className="space-y-1">
              <label>{t("progressiveWebApp.showToolbar")}</label>
              <p className="text-sm text-base-content/70">
                {t("progressiveWebApp.showToolbarDescription")}
              </p>
            </div>
            <input
              type="checkbox"
              checked={!!getValues("showToolbar")}
              onChange={(e) => setValue("showToolbar", e.target.checked)}
              className="checkbox"
            />
          </div>
        </div>

        <div className="mt-4">
          <a
            href="https://docs.floorp.app/docs/features/how-to-use-pwa"
            className="text-primary hover:underline text-sm"
          >
            {t("progressiveWebApp.learnMore")}
          </a>
        </div>
      </CardContent>
    </Card>
  );
}
