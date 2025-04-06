import {
    Card,
    CardContent,
    CardHeader,
    CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { Sliders } from "lucide-react";

export function UICustomization() {
    const { t } = useTranslation();
    const { getValues, setValue } = useFormContext();

    // タブバーのスタイルを取得
    const currentTabbarStyle = getValues("style");

    return (
        <Card>
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Sliders className="size-5" />
                    {t("design.uiCustomization.title")}
                </CardTitle>
            </CardHeader>
            <CardContent className="space-y-6">
                <div>
                    <h3 className="text-base font-medium mb-2">
                        {t("design.uiCustomization.navbar.title")}
                    </h3>
                    <div className="space-y-4">
                        <div>
                            <label className="block mb-2">
                                {t("design.uiCustomization.navbar.positionWithExperiment")}
                            </label>
                            <div className="flex space-x-4">
                                <label className="flex items-center space-x-2">
                                    <input
                                        type="radio"
                                        name="navbarPosition"
                                        value="top"
                                        checked={getValues("navbarPosition") === "top"}
                                        onChange={(e) => setValue("navbarPosition", e.target.value)}
                                    />
                                    <span>{t("design.uiCustomization.navbar.top")}</span>
                                </label>
                                <label className="flex items-center space-x-2">
                                    <input
                                        type="radio"
                                        name="navbarPosition"
                                        value="bottom"
                                        checked={getValues("navbarPosition") === "bottom"}
                                        onChange={(e) => setValue("navbarPosition", e.target.value)}
                                    />
                                    <span>{t("design.uiCustomization.navbar.bottom")}</span>
                                </label>
                            </div>
                        </div>

                        <div className="flex items-center justify-between gap-2">
                            <label htmlFor="search-bar-top">
                                {t("design.uiCustomization.navbar.searchBarTop")}
                            </label>
                            <Switch
                                id="search-bar-top"
                                checked={!!getValues("searchBarTop")}
                                onChange={(e) => setValue("searchBarTop", e.target.checked)}
                            />
                        </div>
                    </div>
                </div>

                {/* 表示カスタマイズ */}
                <div>
                    <h3 className="text-base font-medium mb-2">
                        {t("design.uiCustomization.display.title")}
                    </h3>
                    <div className="space-y-4">
                        <div className="flex items-center justify-between gap-2">
                            <label htmlFor="disable-fullscreen-notification">
                                {t("design.uiCustomization.display.disableFullscreenNotification")}
                            </label>
                            <Switch
                                id="disable-fullscreen-notification"
                                checked={!!getValues("disableFullscreenNotification")}
                                onChange={(e) => setValue("disableFullscreenNotification", e.target.checked)}
                            />
                        </div>

                        <div className="flex items-center justify-between gap-2">
                            <label htmlFor="delete-browser-border">
                                {t("design.uiCustomization.display.deleteBrowserBorder")}
                            </label>
                            <Switch
                                id="delete-browser-border"
                                checked={!!getValues("deleteBrowserBorder")}
                                onChange={(e) => setValue("deleteBrowserBorder", e.target.checked)}
                            />
                        </div>

                        <div className="flex items-center justify-between gap-2">
                            <label htmlFor="hide-unified-extensions-button">
                                {t("design.uiCustomization.display.hideUnifiedExtensionsButton")}
                            </label>
                            <Switch
                                id="hide-unified-extensions-button"
                                checked={!!getValues("hideUnifiedExtensionsButton")}
                                onChange={(e) => setValue("hideUnifiedExtensionsButton", e.target.checked)}
                            />
                        </div>
                    </div>
                </div>

                {/* 特殊機能 */}
                <div>
                    <h3 className="text-base font-medium mb-2">
                        {t("design.uiCustomization.special.title")}
                    </h3>
                    <div className="space-y-4">
                        <div className="flex items-center justify-between gap-2">
                            <label htmlFor="optimize-for-tree-style-tab">
                                {t("design.uiCustomization.special.optimizeForTreeStyleTab")}
                            </label>
                            <Switch
                                id="optimize-for-tree-style-tab"
                                checked={!!getValues("optimizeForTreeStyleTab")}
                                onChange={(e) => setValue("optimizeForTreeStyleTab", e.target.checked)}
                            />
                        </div>

                        <div className="flex items-center justify-between gap-2">
                            <label htmlFor="hide-forward-backward-button">
                                {t("design.uiCustomization.special.hideForwardBackwardButton")}
                            </label>
                            <Switch
                                id="hide-forward-backward-button"
                                checked={!!getValues("hideForwardBackwardButton")}
                                onChange={(e) => setValue("hideForwardBackwardButton", e.target.checked)}
                            />
                        </div>

                        <div className="flex items-center justify-between gap-2">
                            <label htmlFor="stg-like-workspaces">
                                {t("design.uiCustomization.special.stgLikeWorkspaces")}
                            </label>
                            <Switch
                                id="stg-like-workspaces"
                                checked={!!getValues("stgLikeWorkspaces")}
                                onChange={(e) => setValue("stgLikeWorkspaces", e.target.checked)}
                            />
                        </div>
                    </div>
                </div>

                {/* マルチロータブ関連 */}
                {currentTabbarStyle === "multirow" && (
                    <div>
                        <h3 className="text-base font-medium mb-2">
                            {t("design.uiCustomization.multirowTab.title")}
                        </h3>
                        <div className="space-y-4">
                            <div className="flex items-center justify-between gap-2">
                                <label htmlFor="multirow-tab-newtab-inside">
                                    {t("design.uiCustomization.multirowTab.newtabInsideEnabled")}
                                </label>
                                <Switch
                                    id="multirow-tab-newtab-inside"
                                    checked={!!getValues("multirowTabNewtabInside")}
                                    onChange={(e) => setValue("multirowTabNewtabInside", e.target.checked)}
                                />
                            </div>
                        </div>
                    </div>
                )}
            </CardContent>
        </Card>
    );
} 