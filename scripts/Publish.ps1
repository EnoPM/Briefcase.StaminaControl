[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Version,[Parameter(Mandatory=$true)][string]$Commit,[Parameter(Mandatory=$true)][string]$Repository,[switch]$Draft)
$ErrorActionPreference='Stop'
# One verifier for both operating systems and all publication entry points.
$arguments=@((Join-Path $PSScriptRoot 'publish.py'),'--version',$Version,'--commit',$Commit,'--repository',$Repository)
if($Draft){$arguments+='--draft'}
& python @arguments
if($LASTEXITCODE){throw 'Private two-platform publication failed; inspect any retained draft.'}
