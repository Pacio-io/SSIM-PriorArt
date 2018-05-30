<?php /* Copyright 2011-2013 Braiins Ltd

Admin/www/UK-GAAP-DPL/DimensionsList.php

Lists The Dimensions

History:
11.07.11 Started
05.04.13 ShortName -> Name
21.04.13 Braiins Dimension removed

*/
require 'BaseTx.inc';
require Com_Inc_Tx.'ConstantsRg.inc';
require Com_Str_Tx.'DimNamesA.inc';  # $DimNamesA
Head("Dimensions List: $TxName", true);

echo "<h2 class=c>$TxName Dimensions</h2>
<p class=c>For Dimension Members see <a href=DimensionsMap.php>Dimensions Map</a>.</p>
<table class=mc>
<tr class='b bg0'><td>Id</td><td class=c>Id<br>Chr</td><td class=c>Tx<br>Id</td><td>Tx Name</td><td>Braiins Name</td><td>Label</td><td>Role</td></tr>
";

$res = $DB->ResQuery("Select D.*,E.name,T.Text From Dimensions D Join Elements E on E.Id=D.ElId Join Text T on T.Id=E.StdLabelTxtId");
while ($o = $res->fetch_object()) {
  $dimId = (int)$o->Id;
  $didC  = htmlspecialchars(IntToChr($dimId));
  $name  = $o->name;
  $label = $o->Text;
  $role = Role($o->RoleId, true); # true = no trailing [id]
  echo "<tr><td class=r>$dimId</td><td class=c>$didC</td><td class=r>$o->ElId</td><td>$name</td><td>{$DimNamesA[$dimId]}</td><td>$label</td><td>$role</td></tr>\n";
}
echo '</table>
';
Footer(true, true);
########

