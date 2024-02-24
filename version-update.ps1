# ===================================
# This script increments the version of the Community Patch in the various files
# that need updating, git-commits the changes, creates a git-tag with the new
# version, then submits them.
# ===================================

param(
    [switch]$major = $false,
    [switch]$minor = $false,
    [switch]$patch = $false,
    [switch]$skipgit = $false
)

# User must specify how the version is being incremented
if ((-not $major) -and (-not $minor) -and (-not $patch))
{
    Write-Error "Please specify exactly one of -major, -minor, or -patch."
    Exit -1
}

# User cannot specify more than one incrementation method
if (($major -and $minor) -or ($minor -and $patch) -or ($major -and $patch))
{
    Write-Error "Please specify exactly one of -major, -minor, or -patch."
    Exit -1
}

# PowerShell Core 7 or higher is required
if ($PSVersionTable.PSVersion.Major -lt 7)
{
    Write-Error "This script requires PowerShell version 7 or later."
    Exit -1
}

# Need Git
if ((Get-Command "git" -ErrorAction SilentlyContinue) -eq $null)
{
    Write-Error "Git is either not installed or not in your PATH."
    Exit -1
}

$current_dir = Get-Location

# Check if Info.json exists
$info_json_path = Join-Path $current_dir Info.json
if (-not (Test-Path -Path $info_json_path -PathType leaf))
{
    Write-Error "Info.json is missing!"
    Exit -1
}

# Check if AssemblyInfo.cs exists
$assembly_info_path = Join-Path $current_dir "Properties" "AssemblyInfo.cs"
if (-not (Test-Path -Path $assembly_info_path -PathType leaf))
{
    Write-Error "AssemblyInfo.cs is missing!"
    Exit -1
}

# Read Info.json
$info_json_contents = Get-Content -Path $info_json_path
if ($info_json_contents.Count -lt 1)
{
    Write-Error "Info.json is empty!"
    Exit -1
}

# Find current version
$version_string = ""
$version_format_regex = '[0-9]+\.[0-9]+\.[0-9]+'
$version_line = -1
for($i = 0; $i -lt $info_json_contents.Count; $i++)
{
    $line = $info_json_contents[$i]
    if ($line -match (-join ('"Version": "', $version_format_regex)))
    {
        $version_line = $i
        # Extract version number by removing everything else from the line
        $version_string = $line.Replace('"Version": "', "").Replace('"', "").Replace(",", "").Trim()
        break
    }
}

# Make sure we actually found the version
if ($version_line -eq -1)
{
    Write-Error "Info.json is missing version info!"
    Exit -1
}
elseif (-not ($version_string -match $version_format_regex))
{
    Write-Error "Info.json version info malformed: $version_string!"
    Exit -1
}
else
{
    Write-Output "Found current version $version_string."
}

# Split version into three ints for incrementation
$split_version = $version_string.Split(".")
$version_major = [int]::Parse($split_version[0])
$version_minor = [int]::Parse($split_version[1])
$version_patch = [int]::Parse($split_version[2])

# Increment version according to type
if ($major)
{
    $version_major++
    $version_minor = 0
    $version_patch = 0
}
elseif ($minor)
{
    $version_minor++
    $version_patch = 0
}
else #patch
{
    $version_patch++
}

# Format new version string
$new_version = "$version_major.$version_minor.$version_patch"
Write-Output "Version is now $new_version."

# Replace version in Info.json
$info_json_contents[$version_line] = $info_json_contents[$version_line].Replace($version_string, $new_version)
Set-Content -Path $info_json_path -Value $info_json_contents

# Replace version in AssemblyInfo.cs
$assembly_info_contents = Get-Content -Path $assembly_info_path -Raw
$assembly_info_contents = $assembly_info_contents.Replace("[assembly: AssemblyVersion(`"$version_string.0`")]", "[assembly: AssemblyVersion(`"$new_version.0`")]")
$assembly_info_contents = $assembly_info_contents.Replace("[assembly: AssemblyFileVersion(`"$version_string.0`")]", "[assembly: AssemblyFileVersion(`"$new_version.0`")]")
$assembly_info_contents = $assembly_info_contents.Trim()
Set-Content -Path $assembly_info_path -Value $assembly_info_contents

# Get this all sorted for git
if (-not $skipgit)
{
    git add "./Info.json" "./Properties/AssemblyInfo.cs"
    git commit -m "Version updated to $new_version"
    git tag -a $new_version -m "$new_version"
    git push
    git push origin $new_version
}

Write-Output "Version update complete and submitted!"
Exit 0