<?php /* Copyright 2011-2013 Braiins Ltd

Braiins/Rg/RgRun.inc

13.03.13 Added locking

Is required to run a compiled format, finishing with:
 - the generated accounts in $Html and RT messages in $RunMsg
 - $FO as the Formats DB obj for format FormatId with cols FTypeN,FileName,Name
 - $FTypeN    as the type of the format
 - $FFileName as the file name of the format

The enclosing module which requires this needs to:
  - Have loaded Constants.inc
  - Have connected to the Braiins DB, and loaded LoadFuncs.inc or LoadFuncsSA.inc
  - Set $AgentId and $EntityId
  - Set $AppEnum to the enum of the app running the format e.g. APP_Admin, BDT_CFS ...
  - Set $FormatId to the Formats.Id of the format to be run
  - Have defined LOG_FILE using an absolute path.
  - chdir(RG_Path) to move to Braiins/Rg
  - process the final 'returned' $Html and $RunMsg

ToDo
====
- BRO Descriptions: Merge entity into Agent into Braiins Master

- Use
  $DGsInUse

- Agent option for zero money

- HighestPaidDirector as per DimensionHandling.txt
- check requires

When counting directors add test for current at report date

# djh?? But what about BroId_Officers_DirectorSigningReport ??

$Country = trim(substr($Country, 0, $p)); # djh?? Wales for England & Wales if RO address is in Wales?

*/

$startUtime= utime();
require Com_Inc_Tx.'ConstantsRg.inc';
require Com_Inc.'ClassBro.inc';     # $BroInfoA $DiMesA $BroNamesA $BroShortNamesA $DiMeNamesA $DiMeTargetsA $RestatedDiMeTargetsA $TuMesA
require Com_Str_Tx.'RgRT.inc';      # $DiMesRA $NamespacesRgA $TupFullNamesA $BroDescrsA $BroTagsA
require Com_Inc.'DateTime.inc';
require Com_Inc.'FuncsBraiins.inc'; # for UpdateEntity() at the end
require './RgRunFuncs.inc';


$BracketedWordPairsA = [
'asset'       => 'liability',
'assets'      => 'liabilities',
'back'        => 'off',
'charge'      => ['income', 'relief'],
'credit'      => ['charge', 'expense'],
'credited'    => 'charged',
'debts'       => 'funds',
'decrease'    => 'increase',
'deficit'     => 'surplus',
'expense'     => 'income',
'from'        => 'to',
'funds'       => ['debts', 'debt'],
'gain'        => 'loss',
'gains'       => ['charges', 'costs', 'losses'], # losses added
'higher'      => 'lower',
'income'      => ['charges', 'expense'],
'increase'    => ['decrease', 'reduction'],
'lapsed'      => 'granted',
'liabilities' => 'assets',
'liability'   => 'asset',
'loss'        => ['profit', 'gain', 'earnings', 'impairment'],
'losses'      => 'gains',
'lost'        => 'resurrected',
'lower'       => 'higher',
'outflow'     => 'flow',
'outflows'    => 'flows',
'outgoings'   => 'proceeds',
'overdrafts'  => 'cash',
'payable'     => 'receivable',
'payments'    => ['earnings', 'receipts'],
'prepaid'     => 'outstanding', # added
'pre-paid'    => 'outstanding',
'purchase'    => 'sale',
'receipt'     => 'payment',
'receipts'    => 'payments',
'receivable'  => 'payable',
'recoverable' => 'payable',
'release'     => 'charge',
'relief'      => 'tax',
'reversal'    => ['impairment', 'loss'],
'surplus'     => ['deficit', 'obligations'],
'to'          => 'from',
];

# RT constants

# $XrefsA inner array indices
const XrefI_Bits = 0;
const XrefI_Page = 1;
const XrefI_Text = 2;

# BroLoop inner array indices
const BroLoopI_Ref         = 0;
const BroLoopI_NumLoops    = 1;
const BroLoopI_Type        = 2; # 0 if BroId loop as for addresses, 1 if ManDiMeId loop as for Directors
const BroLoopI_TestIdDelta = 3;

# $ContextsA inner array indices
const CI_ContextN  = 0;
const CI_Year      = 1;
const CI_DiMeIdsA  = 2;

# Officers Type Bit Settings derived from dimensions
const OTB_Chairman            =  1; # DiMeId_Chairman
const OTB_CEO                 =  2; # DiMeId_CEO
const OTB_ChairmanCEO         =  4; # DiMeId_ChairmanCEO
const OTB_CoSecDirector       =  8; # DiMeId_CoSecDirector
const OTB_HighestPaidDirector = 16; # DiMeId_HighestPaidDirector
const OTB_Executive           = 32; # DiMeId_ExecutiveOfficer
const OTB_Nonexec             = 64; # DiMeId_NonexecOfficer

# Globals
# =======
$HeaderFn = $FooterFn = null;
$CentreB  = $Year = $TableA = 0; # $TableA is an array while a table is being processed.
$HeadingsA =     # Headings - union of Agent data and master from AgentData table
$PreferencesA =  # Preferences - union of Agent data and master from AgentData table
$BrosA      =    # Bros     [year => [BroId => BrO]]
$BroDatA    =    # Bro data [year => [BroRef => Dat]] where BroRef = BroId{,Bro Object BroDat/BroTupDat key} where the key is omitted if it is just BroDatT_End (2).
$NamedRowsA =    # row name => table row reference
$PropBitsA  =    # array of ManDiMeId => PropBits for Officers and ...
$MMapOffsetsA =  # array of M type DiMeIds offsets for CoSec/CoSecDirector, Accountant/Auditor
$P1LinesA  =     # array of first pass lines, as generated by the formats i.e. after the head and hidden sections
$XrefsA    =     # array of Xrefs arrays
$ContextsA =     # array of contexts used as ref => [CI_* as above]
$TagsA     =     # array of tags generated by BroRef
$TuplesA   = []; # [TupId][TupleID] = 1 where TupleID = "T.n.i.Y#" where n is running tuple use # and i is the instance
$BroYearsA = array_fill(0, 7, 0); # [year => b] set for years with BrosA data djh?? Used?
$RestatedYearsA = [0,0,0,0]; # to record standard years for which there is restated data

$RunMsg    = '';

# Get the format Info
$FO = $DB->ObjQuery("Select FTypeN,FileName,Name From Formats Where Id=$FormatId");
$FTypeN    = (int)$FO->FTypeN;
$FFileName = $FO->FileName;

# Get Read locks for Agent, Entity and Bros
$DB->GetBDbLocks([
  [T_B_Agents,   $AgentId,  Lock_Read],
  [T_B_Entities, $EntityId, Lock_Read],
  [T_B_Bros,     $EntityId, Lock_Read]
], function($why){Error("Sorry, Financial Statements cannot be generated currently as $why.");});

# Get Agent Info from AgentData records, ADT_AgentInfo for just AgentId, ADT_Headings & ADT_Preferences for both AgentId and 1 = Braiins Master, with
# Headings and Preferences for Agent to be merged with the Braiins Masters
$res = $DB->ResQuery("Select AgentId,TypeN,Data From AgentData Where AgentId=$AgentId Or (AgentId=1 And TypeN>1) Order by AgentId Desc"); # TypeN>1 is TypeN > ADT_AgentInfo
while ($o = $res->fetch_object())
  switch ($o->TypeN) {
    case ADT_AgentInfo:   $AgentInfoA   = json_decode($o->Data, true); break;
    case ADT_Headings:    $HeadingsA    = $HeadingsA    + json_decode($o->Data, true); break; # + for array union needs Order by AgentId Desc above so that the Agent data is on the left of the + with the master
    case ADT_Preferences: $PreferencesA = $PreferencesA + json_decode($o->Data, true); break;
  }
$res->free();

# Get Entity Info
$EA = $DB->AaQuery("Select Ref,EName,ETypeId,ESizeId,CurrYear,DataState,AcctsState,DGsInUse From Entities Where Id=$EntityId");
# -> $EName, $ETypeId, $ESizeId, $CurrYear, $DataState, $AcctsState, $DGsInUse
# djh?? Add TxnId check? Add Level check?
$ERef           =      $EA['Ref']; # Used by CFD.php
$EName          =      $EA['EName'];
$ETypeId        = (int)$EA['ETypeId'];
$ESizeId        = (int)$EA['ESizeId'];
$CurrYear       = (int)$EA['CurrYear'];
$DataState      = (int)$EA['DataState'];
$AcctsState     = (int)$EA['AcctsState'];
$DGsInUse = (int)$EA['DGsInUse']; # djh?? Put this to use

# Read and build the Bros
for ($year=0; $year<=Max_RelYear; ++$year) # All years 0,1,2,3 plus pya in 4,5,6 as years
  $BrosA[$year] = $BroDatA[$year] = [];
$res = $DB->ResQuery("Select EntYear,BroId,BroStr From Bros Where EntityId=$EntityId Order By BroId");
$DB->RelBDbLocks(); # Release the Read locks
while ($o = $res->fetch_object()) {
  $year  = $CurrYear - (int)$o->EntYear; # relYear
  $broId = (int)$o->BroId;
  $BrosA[$year][$broId] = $brO = NewBroFromString($broId, $o->BroStr);
  if ($brO->IsMaster())
     foreach ($brO->InfoA[BroI_SlaveIdsA] as $slaveId) # Create the non Set Slaves of this master. Not Set ones because Set Slaves are stored to avoid the need for summing here.
      if ($BroInfoA[$slaveId][BroI_Bits] & BroB_Ele)
        $BrosA[$year][$slaveId] = $brO->CopyToSlave(new Bro($slaveId));
  $BroYearsA[$year] = 1;
  if ($year > Pya_Year_Offset)
    $RestatedYearsA[$year - Pya_Year_Offset] = 1;
}
$res->free();

# Build BroDatA from the Bros.
# As an alternative to the use of BroDatA the Bros could ne accessed directly.
# However, that would require two isset() checks, one for the Bro and then a second isset() for the BroDat key, so presumeably use of BroDatA with one isset() will be faster.
# $BroDatA = # Bro data [year => [BroRef => Dat]] where BroRef = BroId{,Bro Object BroDat/BroTupDat key} where the key is omitted if it is just BroDatT_End (2).
foreach ($BroYearsA as $year => $b) {
  if (!$b) continue;
  $yrDatA = [];
  foreach ($BrosA[$year] as $broId => $brO)
    foreach ($brO->AllBroDatOs() as $broDatKey => $datO)
      # BroDatKey for End Base No Inst is just BroDatT_End => just broId as BroRef
      # BroRef = just int BroId for End case with no DiMes and no Inst, otherwise BroId,BroDatKey
      $yrDatA[$broDatKey === BroDatT_End ? $broId : "$broId,$broDatKey"] = $datO->Dat2 ? : $datO->Dat;
  $BroDatA[$year] = $yrDatA;
}

$ETypeChr    = IntToChr($ETypeId);
$entityNameS = Data(BroId_Info_EntityName);
if ($entityNameS) {
  if ($entityNameS != $EName)
    $RunMsg .= "\nEntity Name on file of <i>$EName</i> does not match data name of <i>$entityNameS</i>. Data name used.";
}else{
  $RunMsg .= "\nNo Entity Name in data. File name of <i>$EName</i> used.";
  $entityNameS = $BroDatA[0][BroId_Info_EntityName] = $EName;
}

# Globals
# 8228,558 SchInputEntity.LegalForm.CountryFormationOrIncorporation:Countries.England
/* djh?? Fudge this for now as below
$Country = $IncorporationCountryS = Data(BroId_Info_IncorporationCountry); # $IncorporationCountryS = Built in variable for full text format use
if ($p = strpos($Country, '&'))
  $Country = trim(substr($Country, 0, $p)); # djh?? Wales for England & Wales if RO address is in Wales?
$Country = ucwords(strtolower($Country));
switch ($Country) {
  case 'England':          $CountryDiMeId = DiMeId_Countries_England;   break;
  case 'Scotland':         $CountryDiMeId = DiMeId_Countries_England+1; break;
  case 'Wales':            $CountryDiMeId = DiMeId_Countries_England+2; break;
  case 'Northern Ireland': $CountryDiMeId = DiMeId_Countries_England+3; break;
  case 'Isle Of Man':      $CountryDiMeId = DiMeId_Countries_England+4; break;
  case 'Jersey':           $CountryDiMeId = DiMeId_Countries_England+5; break;
  case 'Guernsey':         $CountryDiMeId = DiMeId_Countries_England+6; break;
  default:                 $CountryDiMeId = DiMeId_Countries_England-1; break; # United Kingdom
} */

$Country = $IncorporationCountryS = 'England';
$CountryDiMeId = DiMeId_Countries_England;

/* Set up the built in variables and build the signing hidden tags to avoid needed to go thru these ops again later
IncorporationCountryS
DirectorsS
DirectorsApostropheS
DraftS
FullAccountsS
CoSecStatusS
ETypeI
CoSizeI
NumDirectorsI
NumDirectorsSigningAccountsI
CoSecDirectorI
AccountantsAndAuditorsSameB

/*
# djh?? But what about BroId_Officers_DirectorSigningReport ?????

*/
$ETypeI  = $ETypeId;
$CoSizeI = $ESizeId;
$signingHiddenTags = '';
$NumDirectorsI  = $NumDirectorsSigningAccountsI = $CoSecDirectorI = 0;
$MDiMeIdsToMapA =     # array of MDiMeIds to be mapped e.g. [DiMeId_CoSec, DiMeId_Accountants], or just one of them, or null if none
$MMapOffsetsA = null; # array of MDiMeId offsets corresponding to $MDiMeIdsToMapA
for ($manDiMeId=DiMeId_Director1,$d=1; $manDiMeId<=DiMeId_Director40; ++$manDiMeId,++$d) {
  $broRef = BroId_Officers_Name . ",$manDiMeId"; # assuming name has only a manDiMeId i.e. not restated etc.
  #echo "Data($broRef)=",Data($broRef), '<br>';
  if ($name = Data($broRef)) {
    $propBits = 0;
    $bitVal = 1;
    foreach ($ManagedPropDiMeIdsA[MType_Director] as $diMeId) { # DiMeId_Chairman, DiMeId_CEO, DiMeId_ChairmanCEO, DiMeId_CoSecDirector, DiMeId_HighestPaidDirector, DiMeId_ExecutiveOfficer, DiMeId_NonexecOfficer
      if (Data(BroId_Officers_Properties . ",$manDiMeId,$diMeId"))
        $propBits += $bitVal;
      $bitVal *= 2;
    }
    #echo "$d $name propBits=$propBits from broRef $broRef<br/>";
    ++$NumDirectorsI;
    $ref = "Director{$d}Y0";
    if (($propBits & OTB_CoSecDirector) && !$CoSecDirectorI) { # only once in case of more than one being set, tho data checking should check for this
      $CoSecDirectorI = $d;
      $MDiMeIdsToMapA[] = DiMeId_CoSec;
      $MMapOffsetsA[]   = $manDiMeId - DiMeId_CoSec; # offset = DiMeId for the current director $d - the CoSec DiMeId
      $CoSecStatusS   = 'Director';
      $signingHiddenTags .= "\n<ix:nonNumeric name='uk-direp:DirectorSigningReport' contextRef='$ref'/>"; # change /> to ></ix:nonNumeric> to avoid all yellow in VT Viewer
    }
    if (Data(BroId_Officers_DirectorSigningAccounts . ",$manDiMeId")) { # SchInputEntity.Officers.DirectorSigningAccounts:.Director#
      $NumDirectorsSigningAccountsI++;
      $signingHiddenTags .= "\n<ix:nonNumeric name='uk-gaap:NameDirectorSigningAccounts' contextRef='$ref'/>";
    } #                 CI_ContextN, CI_Year,  CI_DiMeIdsA
    $ContextsA[$ref] = [RGC_Officer,       0, [$manDiMeId]];
    $PropBitsA[$manDiMeId] = $propBits;
  }
}
if (!$CoSecDirectorI) {
  # No mapping i.e. no $MDiMeIdsToMapA and$MMapOffsetsA entries.
  $CoSecStatusS = 'Company Secretary';
  $ref = 'CoSecY0';
  $signingHiddenTags .= "\n<ix:nonNumeric name='uk-direp:DirectorSigningReport' contextRef='$ref'></ix:nonNumeric>";
  $ContextsA[$ref] = [RGC_Officer, 0, [DiMeId_CoSec]];
}

# Accountants or Auditors or Same?
$AccountantsAndAuditorsSameB = false;
if ($AuditedB = Data(BroId_Info_Audited)) {
  # Audited
  $FullAccountsS = $HeadingsA['AccountsFullAuditedH'];
  if (Data(BroId_TPAs_AccountantsAndAuditors)) {
    $AccountantsAndAuditorsSameB = true;
    $MDiMeIdsToMapA[] = DiMeId_Accountants;
    $MMapOffsetsA[]   = 1; # TPAType.Auditors is next after TPAType.Accountants -> offset = 1
  }
}else
  $FullAccountsS = $HeadingsA['AccountsFullUnauditedH'];
$DraftS = $FTypeN == RGFT_Stat ? '{Draft}' : '';

if ($NumDirectorsI>1) {
  $DirectorsS = 'Directors';
  $DirectorsApostropheS = 'Directors&#8217;';
}else{
  $DirectorsS = 'Director';
  $DirectorsApostropheS = 'Director&#8217;s';
}

########################################################################
# Include the compiled format which performs Pass 1 to build $P1LinesA #
########################################################################
require "./Exes/$FFileName.inc"; # The compiled format
RgFooter();

##################
# Passes 2 and 3 #
##################
require "./RgHead-$TxName.inc"; # starts the Pass 2 ouput in $P2LinesA[]
require './RgRun2.inc';         # completes Passes 2 and 3

#######
# End # Update and EntityTrans
#######
# $FTypeN = Format Type enum  RGFT_Stat, RGFT_Report, RGFT_Check
$actioN2 = $info2 = 0;
$Type = '';
switch($FTypeN) {
  case RGFT_Stat:
    if ($RunMsg || $DataState != EDSB_OK) {
      # Draft
      $acctsState = ($AcctsState & EASB_Draft) ? $AcctsState : EASB_Draft; # unchanged if already Draft, zap EASB_Down on a change
      $Type = $draftTo = 'Draft ';
    }else{
      # Final Candidate or Final
      if ($AcctsState & EASB_Final) { # EASB_Final has been set for the Entity and no RunMsgs so accept as Final
        $acctsState = $AcctsState;
        $Type = 'Final ';
        $draftTo = '';
      }else{
        $acctsState = ($AcctsState & EASB_FinalCand) ? $AcctsState : EASB_Draft; # unchanged if already EASB_FinalCand, zap EASB_Down on a change
        $Type = 'Final Candidate ';
        $draftTo = 'Draft ';
      }
    }
    $Html = str_replace('{Draft}', $draftTo, $Html);
    if ($acctsState != $AcctsState) {
      # Get Entity Write lock for the update
      $DB->GetBDbLock(T_B_Entities, $EntityId, Lock_Write, function($why){Error("Sorry, the Entity is not currently available for updating after Financial Statement generation as $why.");});
      $DB->autocommit(false);
      UpdateEntity($EntityId, 'AcctsState', $acctsState, $EA); # $EA is the Entity info AA read above
      AddEntityTran($EntityId, $CurrYear, $AppN, ETA_RgStat, $FormatId, ETA_SetAcctsState, $acctsState);
      $DB->commit();
      $DB->RelBDbLocks(); # Release the Write lock
    }
    $t = $Type.'Financial Statements generated';
    break;
  case RGFT_Report: $t = 'Report generated'; break;
  case RGFT_Check:  $t = 'Check run';        break; # djh?? Add DataState update?
}
$RunMsg .= "\n$t in " . ElapsedTime($startUtime);

