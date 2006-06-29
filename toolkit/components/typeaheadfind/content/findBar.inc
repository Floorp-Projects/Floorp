    <toolbar id="FindToolbar" hidden="true" align="center" fullscreentoolbar="true">
      <toolbarbutton id="find-closebutton" tooltiptext="&findCloseButton.tooltip;"
                     oncommand="gFindBar.closeFindBar();"/>

      <label control="find-field" id="find-label" class="find-fast"/>
      <hbox id="find-field-container" class="find-fast">
        <textbox id="find-field" oninput="gFindBar.find(this.value);"
                onkeypress="gFindBar.onFindBarKeyPress(event);"
                onfocus="gFindBar.onFindBarFocus();"
                onblur="gFindBar.onFindBarBlur();"
                oncompositionstart="gFindBar.onFindBarCompositionStart(event);"
                oncompositionend="gFindBar.onFindBarCompositionEnd(event);"/>
      </hbox>
      <toolbarbutton id="find-next" label="&next.label;" tooltiptext="&next.tooltip;"
                     command="cmd_findAgain" disabled="true" accesskey="&next.accesskey;"/>
      <toolbarbutton id="find-previous" label="&previous.label;" tooltiptext="&previous.tooltip;"
                     command="cmd_findPrevious" disabled="true" accesskey="&previous.accesskey;"/>
      <toolbarbutton id="highlight" label="&highlight.label;"
                     tooltiptext="&highlight.tooltiptext;"
                     oncommand="gFindBar.toggleHighlight(!this.checked);
                                if (gFindBar.mFindMode != FIND_NORMAL)
                                  gFindBar.setFindCloseTimeout();"
                     type="checkbox" disabled="true"
                     accesskey="&highlight.accesskey;"/>
      <label id="match-case-status" class="find-fast"/>
      <image id="find-status-icon"/>
      <label id="find-status"/>
    </toolbar>
