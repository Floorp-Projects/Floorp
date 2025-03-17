import { useState } from "react";
import { useTheme } from "../ThemeProvider.tsx";
import { useBackground } from "../../hooks/useBackground.ts";

const defaultImages = {
  light: "/assets/backgrounds/light.jpg",
  dark: "/assets/backgrounds/dark.jpg",
};

export function BackgroundSetting() {
  const { resolvedTheme } = useTheme();
  const [customBackground, setCustomBackground] = useState<string | null>(null);

  const handleImageUpload = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (file) {
      const reader = new FileReader();
      reader.onloadend = () => {
        setCustomBackground(reader.result as string);
      };
      reader.readAsDataURL(file);
    }
  };

  const currentBackground = customBackground || defaultImages[resolvedTheme];
  useBackground(currentBackground);

  return (
    <div className="grid-item">
      <div
        className="w-full h-32 bg-cover bg-center mb-4 rounded-lg"
        style={{ backgroundImage: `url(${currentBackground})` }}
      />
      <input
        type="file"
        accept="image/*"
        onChange={handleImageUpload}
        className="text-sm text-secondary"
      />
      {customBackground && (
        <button
          type="button"
          onClick={() => setCustomBackground(null)}
          className="mt-2 text-sm text-secondary hover:text-primary"
        >
          デフォルトに戻す
        </button>
      )}
    </div>
  );
}
