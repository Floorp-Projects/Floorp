{foreach name=loop from=$items item=i}
{$smarty.foreach.loop.iteration+2}
{$smarty.foreach.loop.iteration+$flt}
{$smarty.foreach.loop.iteration+$obj->six()}
{$smarty.foreach.loop.iteration+$obj->ten}
{/foreach}
{$obj->ten+$flt}
{$obj->ten*$flt}
{$obj->six()+$obj->ten}
{$obj->ten+$obj->ten}
{$obj->six()+$flt}
{$obj->six()+$items.0}
