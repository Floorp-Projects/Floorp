<div id="mBody">
<h1>Extend Firefox Contest Winners</h1>
<p>We are happy to announce the winners in our <a href="http://extendfirefox.com/">Extend Firefox Contest</a>, a contest held to award the best and brightest extension developers in the Firefox community.  The contest asked entrants to create Firefox Extensions that are innovative, useful, and integrate with today's Web services. Over 200 Extensions were submitted to the contest.  Many thanks to everyone who entered and everyone who helped spread the word about the contest.</p>

{foreach key=key item=item from=$winners}
<div class="recommended">
<br class="clear-both"/>
<strong>{$item.prize}</strong>
    <img class="recommended-img" alt="" src="{$config.webpath}/images/finalists/{$screenshots[$item.guid]}"/>
    <h2><a href="./extensions/moreinfo.php?id={$item.id}">{$item.name}</a> <small>by {$item.author}</small></h2>
    <div class="recommended-download">
        <h3><a href="{$item.uri}" onclick="return install(event,'{$item.name} {$item.version}', '{$config.webpath}/images/default.png');" title="Install {$item.name} {$item.version} (Right-Click to Download)">Install Extension ({$item.size}KB)</a></h3>
    </div>
    <p>{$descriptions[$item.guid]}</p>
</div>
{/foreach}
<br class="clear-both" />
</div>
