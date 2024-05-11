import noranekoIconUrl from "@skin/noraneko/cat_hand_black.png";
import type {} from "solid-styled-jsx";

export function menu() {
  return (
    <>
      <div
        class="nora-menu-div"
        style={{
          "margin-inline-start": "34px",
          display: "flex",
          "padding-inline": "10px",
        }}
      >
        <img src={"chrome://noraneko" + noranekoIconUrl} width="30px" />
        <p
          style={{
            "padding-inline-start": "9px",
            "font-size": "1.07em",
            "user-select": "none",
          }}
        >
          noraneko
        </p>
      </div>
      <style jsx dynamic>
        {`
        .nora-menu-div {
          border: 1px solid var(--in-content-primary-button-border-color);
          border-radius: 4px;
        }
        .nora-menu-div:hover {
          background-color: var(--in-content-button-background-hover);
          color: var(--in-content-button-text-color-hover);
          border-color: var(--in-content-button-border-color-hover);
        }
          `}
      </style>
    </>
  );
}
