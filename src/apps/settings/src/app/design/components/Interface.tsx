import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useInterfaceDesigns } from "@/app/design/useInterfaceDesigns.ts";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { Palette } from "lucide-react";

export function Interface() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();
  const interfaceOptions = useInterfaceDesigns();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Palette className="size-5" />
          {t("design.interface")}
        </CardTitle>
      </CardHeader>
      <CardContent>
        <div className="space-y-3">
          <div>
            <p className="text-sm text-base-content/70 mb-4">
              {t("design.interfaceDescription")}
            </p>
            <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-3">
              {interfaceOptions.map((option) => (
                <label
                  key={option.value}
                  tabIndex={0}
                  className={`flex flex-col items-center rounded p-2 cursor-pointer relative border-1 transition-colors ${getValues("design") === option.value
                    ? "bg-primary/10 text-base-content dark:bg-primary/15 border-primary/30 ring-1 ring-primary/20"
                    : "border-secondary/10 hover:bg-base-200 focus-within:bg-base-200 focus-within:border-primary/20 focus:bg-base-200 focus:border-primary/20"
                    }`}
                  onKeyDown={(e) => {
                    if (e.key === "Enter" || e.key === " ") {
                      setValue("design", option.value);
                      e.preventDefault();
                    }
                  }}
                >
                  <img
                    src={option.image}
                    alt={option.title}
                    className="w-40 h-24 mb-2 object-contain drop-shadow-sm"
                  />
                  <span className="text-sm font-medium">{option.title}</span>
                  <input
                    type="radio"
                    name="design"
                    value={option.value}
                    checked={getValues("design") === option.value}
                    onChange={() => setValue("design", option.value)}
                    className="opacity-0 absolute top-2 right-2 focus:opacity-100 focus:outline-none focus:ring-2 focus:ring-primary/40"
                  />
                </label>
              ))}
            </div>
          </div>

          <div className="mt-6">
            <h3 className="text-base font-medium mb-2">
              {t("design.otherInterfaceSettings")}
            </h3>
            <div className="flex items-center justify-between gap-2">
              <div className="space-y-1">
                <label htmlFor="favicon-color">
                  {t("design.useFaviconColorToBackgroundOfNavigationBar")}
                </label>
              </div>
              <Switch
                id="favicon-color"
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
