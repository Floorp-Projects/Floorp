<?xml version="1.0"?>
<RDF:RDF xmlns:RDF="http://www.w3.org/1999/02/22-rdf-syntax-ns#" xmlns:em="http://www.mozilla.org/2004/em-rdf#">

{if $update}
<RDF:Description about="urn:mozilla:{$update.exttype}:{$reqItemGuid}">
    <em:updates>
        <RDF:Seq>
            <RDF:li resource="urn:mozilla:{$update.exttype}:{$reqItemGuid}:{$update.extversion}"/>
        </RDF:Seq>
    </em:updates>
</RDF:Description>

<RDF:Description about="urn:mozilla:{$update.exttype}:{$update.extguid}:{$update.extversion}">
    <em:version>{$update.extversion}</em:version>
    <em:targetApplication>
        <RDF:Description>
            <em:id>{$update.appguid}</em:id>
            <em:minVersion>{$update.appminver}</em:minVersion>
            <em:maxVersion>{$update.appmaxver}</em:maxVersion>
            <em:updateLink>{$update.exturi}</em:updateLink>
        </RDF:Description>
    </em:targetApplication>
</RDF:Description>
{/if}

</RDF:RDF>
