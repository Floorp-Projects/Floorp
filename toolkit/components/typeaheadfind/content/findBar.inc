    <toolbar id="FindToolbar" hidden="true" align="center" fullscreentoolbar="true">
      <toolbarbutton id="find-closebutton" tooltiptext="&findCloseButton.tooltip;"
                     oncommand="closeFindBar();"/>

      <label value="&find.label;"/>
      <hbox id="find-field-container">
        <textbox id="find-field" oninput="find(this.value);" onfocus="onFindBarFocus();"
                onkeypress="onFindBarKeyPress(event);" onblur="onFindBarBlur();"/>
      </hbox>
      <toolbarbutton id="find-next" label="&findNext.label;" tooltiptext="&findNext.tooltip;"
                     command="cmd_findAgain" disabled="true"/>
      <toolbarbutton id="find-previous" label="&findPrevious.label;" tooltiptext="&findPrevious.tooltip;"
                     command="cmd_findPrevious" disabled="true"/>
      <toolbarbutton id="highlight" label="&highlight.label;" tooltiptext="&highlight.tooltip;"
                     oncommand="toggleHighlight(!this.checked);
                                if (gFindMode != FIND_NORMAL) setFindCloseTimeout();"
                     type="checkbox" disabled="true"/>
      <checkbox id="find-case-sensitive" oncommand="toggleCaseSensitivity(this.checked);"
                label="&caseSensitiveCheckbox.label;" accesskey="&caseSensitiveCheckbox.accesskey;"/>
      <image id="find-status-icon"/>
      <label id="find-status"/>
    </toolbar>

