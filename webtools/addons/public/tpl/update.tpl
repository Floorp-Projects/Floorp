<?xml version="1.0"?>
<RDF:RDF xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#" xmlns:em="http://www.mozilla.org/2004/em-rdf#">

{if $update}
<RDF:Description about="urn:mozilla:{$update.exttype}:{$update.extguid|escape:html:"UTF-8"}">
    <em:updates>
        <RDF:Seq>
            <RDF:li resource="urn:mozilla:{$update.exttype}:{$update.extguid|escape:html:"UTF-8"}:{$update.extversion|escape:html:"UTF-8"}"/>
        </RDF:Seq>
    </em:updates>
</RDF:Description>

<RDF:Description about="urn:mozilla:{$update.exttype|escape:html:"UTF-8"}:{$update.extguid|escape:html:"UTF-8"}:{$update.extversion|escape:html:"UTF-8"}">
    <em:version>{$update.extversion|escape:html:"UTF-8"}</em:version>
    <em:targetApplication>
        <RDF:Description>
            <em:id>{$update.appguid|escape:html:"UTF-8"}</em:id>
            <em:minVersion>{$update.appminver|escape:html:"UTF-8"}</em:minVersion>
            <em:maxVersion>{$update.appmaxver|escape:html:"UTF-8"}</em:maxVersion>
            <em:updateLink>{$update.exturi|escape:html:"UTF-8"}</em:updateLink>
            {if $update.hash}
            <em:updateHash>{$update.hash|escape:html:"UTF-8"}</em:updateHash>
            {/if}
        </RDF:Description>
    </em:targetApplication>
</RDF:Description>
{/if}

</RDF:RDF>
