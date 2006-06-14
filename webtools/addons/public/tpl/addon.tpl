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


<p>{$addon->Description|strip_tags|nl2br}</p>

<p class="requires">
Works with:
</p>
<table summary="Compatible applications and their versions.">
{foreach key=key item=item from=$addon->AppVersions}
    {counter assign=count start=0}
    <tr>
        <td><img src="{$config.webpath}/images/{$item.AppName|lower}_icon.png" width="34" height="34" alt=""></td>
        <td>{$item.AppName|escape}</td>
        <td>{$item.MinAppVer|escape} - {$item.MaxAppVer|escape}</td>
        <td>{foreach key=throwaway item=os from=$item.os}{counter assign=count}{if $count > 1}, {/if}{$os|escape}{/foreach}</td>
    </tr>
{/foreach}
</table>
    <div class="key-point install-box">
        <div class="install" id="install-{$addon->ID}">
            {foreach key=key item=item from=$addon->OsVersions}
                {if $item.URI}
                    <div class="{$item.OSName|escape}">
                        <a href="{$item.URI|escape}" onclick="return {$addon->installFunc}(event,'{$addon->Name|escape} {$item.Version|escape}', '{$config.webpath}/images/default.png');" title="Install for {$item.OSName|escape} {$item.Version|escape} (Right-Click to Download)">
                            Install Now
                            {if $multiDownloadLinks}
                                for {$item.OSName|escape}
                            {/if}
                        </a> ({$item.Size|escape} <abbr title="Kilobytes">KB</abbr>)
                    </div>
                {/if}
            {/foreach}
        </div>
    </div>
        <script type="text/javascript">
            var platform = getPlatformName();
            var outer = document.getElementById("install-{$addon->ID}");
            var installs = outer.getElementsByTagName("DIV");
            var found = false;
            for(var i = 0; i < installs.length; ++i) {ldelim}
                if(installs[i].className == platform || installs[i].className == "ALL") {ldelim}
                    found = true;
                {rdelim} else {ldelim}
                    installs[i].style.display = "none";
                {rdelim}
            {rdelim}
            if(!found) {ldelim}
                outer.appendChild(document.createTextNode("{$addon->Name|escape} is not available for "+platform+"."));
            {rdelim}
        </script>
    <div class="install-other">
        <a href="{$config.webpath}/{$app}/{$addon->ID}/history">Other Versions</a>
    </div>

    {if $addon->isThunderbirdAddon}
    <div class="install-thunderbird">
        <p>How to Install in Thunderbird:</p>
        <ol>
        <li>Right-Click the link above and choose "Save Link As..." to Download and save the file to your hard disk.</li>
        <li>In Mozilla Thunderbird, open the extension manager (Tools Menu/Extensions)</li>
        <li>Click the Install button, and locate/select the file you downloaded and click "OK"</li>
        </ol>
    </div>
    {/if}

{if $addon->History[0].Notes}
    <h2>Version Notes</h2>
    <p>{$addon->History[0].Notes|strip_tags|nl2br}</p>
{/if}

{if $addon->devcomments}
<h2>Developer Comments</h2>
<p>{$addon->devcomments|strip_tags|nl2br}</p>
{/if}

<h3 id="user-comments">User Comments</h3>

<p><strong><a href="{$config.webpath}/addcomment.php?aid={$addon->ID}&amp;app={$app}">Add your own comment &#187;</a></strong></p>

<ul id="opinions">
{section name=comments loop=$addon->Comments max=10}
<li>
<div class="opinions-vote">{$addon->Comments[comments].CommentVote} <span class="opinions-caption">out of 5</span></div>
<h4 class="opinions-title">{$addon->Comments[comments].CommentTitle|strip_tags}</h4>
<p class="opinions-info">by 
{if $addon->Comments[comments].CommentName}
{$addon->Comments[comments].CommentName|strip_tags}
{else}
{$addon->Comments[comments].UserName|strip_tags}
{/if}
, {$addon->Comments[comments].CommentDate|date_format}</p>
<p class="opinions-text">{$addon->Comments[comments].CommentNote|strip_tags|nl2br}</p>
<p class="opinions-helpful"><strong>{$addon->Comments[comments].helpful_yes}</strong> out of <strong>{$addon->Comments[comments].helpful_total}</strong> viewers found this comment helpful<br>
Was this comment helpful? <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=yes&amp;app={$app}">Yes</a> &#124; <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=no&amp;app={$app}">No</a></p>
</li>
{/section}
</ul>

<p><strong><a href="{$config.webpath}/{$app}/{$addon->ID}/comments/">Read all comments &#187;</a></strong></p>

<h3>Add-on Details</h3>
<ul>
<li>Categories: 
<ul>
{section name=cats loop=$addon->AddonCats}
<li><a href="{$config.webpath}/search.php?cat={$addon->AddonCats[cats].CategoryID}" title="See other add-ons in this category.">{$addon->AddonCats[cats].CatName}</a></li>
{/section}
</ul>
</li>
<li>Last Updated: {$addon->DateUpdated|date_format}</li>
<li>Total Downloads: {$addon->TotalDownloads} &nbsp;&#8212;&nbsp; Downloads this Week: {$addon->downloadcount}</li>
<li>See <a href="{$config.webpath}/{$app}/{$addon->ID}/history/">all previous releases</a> of this add-on.</li>
{if $addon->Homepage}
<li>View the <a href="{$addon->Homepage}">homepage</a> for this add-on.</li>
{/if}
</ul>

