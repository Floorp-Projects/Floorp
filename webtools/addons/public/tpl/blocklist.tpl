<?xml version="1.0"?>
<blocklist xmlns="http://www.mozilla.org/2006/addons-blocklist">
{if $blocklist}
  <emItems>
{   foreach key=itemGuid item=item from=$blocklist.items}
{       if $item}
    <emItem id="{$itemGuid|escape:html:"UTF-8"}">
{           foreach key=itemId item=itemRange from=$item}
{               if $blocklist.apps.$itemGuid.$itemId}
      <versionRange{if $itemRange.itemMin and $itemRange.itemMax} minVersion="{$itemRange.itemMin|escape:html:"UTF-8"}" maxVersion="{$itemRange.itemMax|escape:html:"UTF-8"}"{/if}>
{                   foreach key=appGuid item=app from=$blocklist.apps.$itemGuid.$itemId}
        <targetApplication{if $appGuid} id="{$appGuid|escape:html:"UTF-8"}"{/if}>
{                       foreach item=appRange from=$app}
{                           if $appRange.appMin and $appRange.appMax}
          <versionRange minVersion="{$appRange.appMin|escape:html:"UTF-8"}" maxVersion="{$appRange.appMax|escape:html:"UTF-8"}"/>
{                           /if}
{                       /foreach}
        </targetApplication>
{                   /foreach}
      </versionRange>
{               else}
      <versionRange minVersion="{$itemRange.itemMin|escape:html:"UTF-8"}" maxVersion="{$itemRange.itemMax|escape:html:"UTF-8"}"/>
{               /if}
{           /foreach}
    </emItem>
{       else}
    <emItem id="{$itemGuid|escape:html:"UTF-8"}"/>
{       /if}
{   /foreach}
  </emItems>
{/if}
</blocklist>
