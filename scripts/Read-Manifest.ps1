param([string]$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')))
$version = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'VERSION')).Trim()
if ($version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Invalid VERSION file' }
$template = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'briefcase.mod.json.in'))
return ($template.Replace('@MOD_VERSION@', $version) | ConvertFrom-Json)
