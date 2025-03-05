import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";

export function Tabbar() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();

  return (
    <Card>
      <CardHeader>
        <CardTitle>{t("design.tabBar")}</CardTitle>
      </CardHeader>
      <CardContent className="space-y-6">
        <div>
          <label className="block mb-2 text-base font-medium">
            {t("design.style")}
          </label>
          <p className="text-sm text-muted-foreground mb-4">
            {t("design.styleDescription")}
          </p>
          <div className="flex space-x-4">
            <label className="flex items-center space-x-2">
              <input
                type="radio"
                name="style"
                value="horizontal"
                checked={getValues("style") === "horizontal"}
                onChange={(e) => setValue("style", e.target.value)}
              />
              <span>{t("design.horizontal")}</span>
            </label>
            <label className="flex items-center space-x-2">
              <input
                type="radio"
                name="style"
                value="multirow"
                checked={getValues("style") === "multirow"}
                onChange={(e) => setValue("style", e.target.value)}
              />
              <span>{t("design.multirow")}</span>
            </label>
          </div>
        </div>

        <div className="mt-4">
          <label className="block mb-2 text-base font-medium">
            {t("design.position")}
          </label>
          <p className="text-sm text-muted-foreground mb-4">
            {t("design.positionDescription")}
          </p>
          <div className="flex flex-col space-y-2">
            {[
              { value: "default", label: "design.default" },
              { value: "hide-horizontal-tabbar", label: "design.hideTabBar" },
              {
                value: "optimise-to-vertical-tabbar",
                label: "design.hideTabBarAndTitleBar",
              },
              {
                value: "bottom-of-navigation-toolbar",
                label: "design.showTabBarOnBottomOfNavigationBar",
              },
              {
                value: "bottom-of-window",
                label: "design.showTabBarOnBottomOfWindow",
              },
            ].map(({ value, label }) => (
              <label key={value} className="flex items-center space-x-2">
                <input
                  type="radio"
                  name="position"
                  value={value}
                  checked={getValues("position") === value}
                  onChange={(e) =>
                    setValue("position", e.target.value)}
                />
                <span>{t(label)}</span>
              </label>
            ))}
          </div>
        </div>

        {getValues("position") === "optimise-to-vertical-tabbar" && (
          <div className="mt-4 p-3 bg-muted rounded-lg">
            <p className="text-sm">
              {t("design.verticalTabIsRemovedFromBrowser")}{" "}
              <a
                href="https://docs.floorp.app/docs/features/tab-bar-customization/#vertical-tabs"
                className="text-primary hover:underline"
              >
                {t("design.learnMore")}
              </a>
            </p>
          </div>
        )}
      </CardContent>
    </Card>
  );
}
