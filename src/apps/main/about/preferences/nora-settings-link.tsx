export function NoraSettingsLink() {
  return (
    <xul:richlistitem
      id="category-nora-link"
      class="category"
      align="center"
      tooltiptext="Nora Settings Link"
      onClick={() => {
        window.location.href = "http://localhost:5183/";
      }}
    >
      <xul:image class="category-icon" />
      <xul:label class="category-name" flex="1">
        Nora Settings Link
      </xul:label>
    </xul:richlistitem>
  );
}
