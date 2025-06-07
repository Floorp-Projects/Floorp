import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/ui/card.tsx";
import { useInterfaceDesigns } from "@/app/design/useInterfaceDesigns.ts";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";

export function Interface() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();
  const interfaceOptions = useInterfaceDesigns();

  return (
    <Card>
      <CardHeader>
        <CardTitle>{t("design.interface")}</CardTitle>
      </CardHeader>
      <CardContent>
        <div className="space-y-6">
          <div>
            <h3 className="text-base font-medium mb-2">
              {t("design.interface")}
            </h3>
            <p className="text-sm text-muted-foreground mb-4">
              {t("design.interfaceDescription")}
            </p>
            <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-3">
              {interfaceOptions.map((option) => (
                <label
                  key={option.value}
                  className={`flex flex-col items-center border rounded p-2 cursor-pointer relative hover:bg-accent ${
                    getValues("design") === option.value
                      ? "bg-accent text-accent-foreground dark:bg-accent dark:text-accent-foreground"
                      : "bg-transparent"
                  }`}
                >
                  <img
                    src={option.image}
                    alt={option.title}
                    className="w-40 h-24 mb-2 object-contain"
                  />
                  <span className="text-sm font-medium">{option.title}</span>
                  <input
                    type="radio"
                    name="design"
                    value={option.value}
                    checked={getValues("design") === option.value}
                    onChange={() => setValue("design", option.value)}
                    className="hidden top-2 right-2"
                  />
                </label>
              ))}
            </div>
          </div>

          {["lepton", "photon", "protonfix"].includes(
            getValues("design") || "",
          ) && (
            <div className="mt-4 p-3 bg-muted rounded-lg">
              <p className="text-sm">
                {t("design.advancedLeptonThemeSettingsDescription")}
                <a
                  href="https://support.floorp.app/docs/features/design-customization"
                  className="text-primary hover:underline"
                >
                  {t("design.advancedLeptonThemeSettings")}
                </a>
              </p>
            </div>
          )}

          <div className="mt-6">
            <h3 className="text-base font-medium mb-2">
              {t("design.otherInterfaceSettings")}
            </h3>
            <div className="flex items-center justify-between">
              <div className="space-y-1">
                <label>
                  {t("design.useFaviconColorToBackgroundOfNavigationBar")}
                </label>
              </div>
              <input
                type="checkbox"
                checked={!!getValues("faviconColor")}
                onChange={(e) => setValue("faviconColor", e.target.checked)}
              />
            </div>
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
