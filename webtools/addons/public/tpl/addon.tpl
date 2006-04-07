<div class="rating" title="{$addon->Rating} out of 5">Rating: {$addon->Rating}</div>
<h2><strong>{$addon->Name}</strong> &raquo; Overview</h2>

{if $addon->PreviewURI}
<p class="screenshot">
<a href="{$config.webpath}/{$app}/{$addon->ID}/previews/" title="See more {$addon->Name} previews.">
<img src="{$config.webpath}{$addon->PreviewURI}" height="{$addon->PreviewHeight}" width="{$addon->PreviewWidth}" alt="{$addon->Name} screenshot">
</a>
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/previews/" title="See more {$addon->Name} previews.">More Previews &raquo;</a></strong>
</p>
{/if}

<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by 
{foreach key=key item=item from=$addon->Authors}
    <a href="{$config.webpath}/{$app}/{$item.UserID|escape}/author/">{$item.UserName|escape}</a>,
{/foreach}
released on {$addon->VersionDateAdded|date_format}
</p>


<p>{$addon->Description|nl2br}</p>

<p class="requires">
Works with:
</p>
<table summary="Compatible applications and their versions.">
{foreach key=key item=item from=$addon->AppVersions}
    {counter assign=count start=0}
    <tr>
        <td><img src="{$config.webpath}/images/{$item.AppName|lower}_icon.png" width="34" height="34" alt="{$item.AppName|escape}"></td>
        <td>{$item.AppName|escape}</td>
        <td>{$item.MinAppVer|escape} - {$item.MaxAppVer|escape}</td>
        <td>{foreach key=throwaway item=os from=$item.os}{counter assign=count}{if $count > 1}, {/if}{$os|escape}{/foreach}</td>
    </tr>
{/foreach}
</table>
    <div class="key-point install-box">
        <div class="install">
            <script type="text/javascript">
            //<![CDATA[
                var installs = new Array(3);
                    /* If an install has a URI, it's available.  Otherwise, you're
                     * out of luck. :-/ */
                    {foreach key=key item=item from=$addon->OsVersions}
                        installs["{$key|escape}"] = "{$item.URI|escape}";
                    {/foreach}

                var platform = getPlatformName();

                document.writeln("<div>");

                if (installs[platform]) {ldelim}
                        document.writeln("<a id=\"install-link\" href=\"" + installs[platform]+ "\" onclick=\"return {$addon->installFunc}(event,'{$addon->Name|escape} {$addon->Version|escape}', '{$config.webpath}/images/default.png');\" title=\"Install for " + platform + " (Right-Click to Download)\">Install Now for " + platform + "</a> ({$item.Size|escape} <abbr title=\"Kilobytes\">KB</abbr>)");
                {rdelim} else if ("{$key|escape}" == "ALL") {ldelim}
                        document.writeln("<a id=\"install-link\" href=\"{$item.URI|escape}\" onclick=\"return {$addon->installFunc}(event,'{$addon->Name|escape} {$addon->Version|escape}', '{$config.webpath}/images/default.png');\" title=\"Install for " + platform + " (Right-Click to Download)\">Install Now for " + platform + "</a> ({$item.Size|escape} <abbr title=\"Kilobytes\">KB</abbr>)");
                {rdelim} else  {ldelim}
                    document.writeln("<strong>{$addon->Name|escape}</strong> is not available for " + platform + ".");
                {rdelim}
                
                document.writeln("</div>");

            //]]>
            </script>
            <noscript>
                {if $multiDownloadLinks}
                    <b>Install Now:</b><br />
                {/if}
                    {foreach key=key item=item from=$addon->OsVersions}
                        {if $item.URI}
                            <div>
                                <a href="{$item.URI|escape}" onclick="return {$addon->installFunc}(event,'{$item.AppName|escape} {$item.Version|escape}', '{$config.webpath}/images/default.png');" title="Install for {$item.OSName|escape} {$item.Version|escape} (Right-Click to Download)">
                                    {if $multiDownloadLinks}
                                        {$item.OSName|escape}
                                    {else}
                                        Install Now
                                    {/if}
                                </a> ({$item.Size|escape} <abbr title="Kilobytes">KB</abbr>)
                            </div>
                        {/if}
                    {/foreach}
            </noscript>
        </div>
    </div>

    <div class="install-other">
        <a href="{$config.webpath}/{$app}/{$addon->ID}/history">Other Versions</a>
    </div>

{if $addon->devcomments}
<h2>Developer Comments</h2>
<p>{$addon->devcomments|nl2br}</p>
{/if}

<h3 id="user-comments">User Comments</h3>

<p><strong><a href="{$config.webpath}/addcomment.php?aid={$addon->ID}&amp;app={$app}">Add your own comment &#187;</a></strong></p>

<ul id="opinions">
{section name=comments loop=$addon->Comments max=10}
<li>
<div class="opinions-vote">{$addon->Comments[comments].CommentVote} <span class="opinions-caption">out of 5</span></div>
<h4 class="opinions-title">{$addon->Comments[comments].CommentTitle}</h4>
<p class="opinions-info">by {$addon->Comments[comments].CommentName}, {$addon->Comments[comments].CommentDate|date_format}</p>
<p class="opinions-text">{$addon->Comments[comments].CommentNote}</p>
<p class="opinions-helpful"><strong>{$addon->Comments[comments].helpful_yes}</strong> out of <strong>{$addon->Comments[comments].helpful_total}</strong> viewers found this comment helpful<br>
Was this comment helpful? <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=yes&amp;app={$app}">Yes</a> &#124; <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=no&amp;app={$app}">No</a></p>
</li>
{/section}
</ul>

<p><strong><a href="{$config.webpath}/{$app}/{$addon->ID}/comments/">Read all comments &#187;</a></strong></p>

<h3>Addon Details</h3>
<ul>
<li>Categories: 
<ul>
{section name=cats loop=$addon->AddonCats}
<li><a href="{$config.webpath}/search.php?cat={$addon->AddonCats[cats].CategoryID}" title="See other Addons in this category.">{$addon->AddonCats[cats].CatName}</a></li>
{/section}
</ul>
</li>
<li>Last Updated: {$addon->DateUpdated|date_format}</li>
<li>Total Downloads: {$addon->TotalDownloads} &nbsp;&#8212;&nbsp; Downloads this Week: {$addon->downloadcount}</li>
<li>See <a href="{$config.webpath}/{$app}/{$addon->ID}/history/">all previous releases</a> of this addon.</li>
{if $addon->UserWebsite}
<li>View the <a href="{$addon->Homepage}">Author's homepage</a> for this addon.</li>
{/if}
</ul>

