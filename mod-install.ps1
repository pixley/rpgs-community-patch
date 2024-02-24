# ===================================
# This script installs the Community Patch to the local RPG Sounds mods folder.
# This is for development use only.
# ===================================

param (
	[switch]$debug = $false		# flag to instead install debug binaries
)

# PowerShell Core 7 or higher is required
if ($PSVersionTable.PSVersion.Major -lt 7)
{
    Write-Error "This script requires PowerShell version 7 or later."
    Exit -1
}

$current_dir = $PSScriptRoot

# Get path to Info.json
$info_json_path = Join-Path $current_dir Info.json
# Now for DLL path
$build_config = $debug ? "Debug" : "Release"
$path_to_binary = Join-Path $current_dir "bin" $build_config "rpgs-patch.dll"

Write-Output "Begin $build_config install to RPG Sounds."

# Authoritative list of files to copy to the mod folder
$files_to_copy = @($path_to_binary, $info_json_path)

# Check to make sure we have everything we need
foreach ($file in $files_to_copy)
{
	if (-not (Test-Path -Path $file -PathType leaf))
	{
		$filename = Split-Path $file -Leaf
	    Write-Error "$filename is missing!"
	    Exit -1
	}
}

# Find path to local RPG Sounds install
$local_rpgs_path_file_path = Join-Path $current_dir "local-rpgs-path.txt"
if (-not (Test-Path -Path $local_rpgs_path_file_path -PathType leaf))
{
	Write-Error "Could not find local-rpgs-path.txt.  Please create it."
	Exit -1
}
$local_rpgs_path = Get-Content -Path $local_rpgs_path_file_path -Raw
if (-not (Test-Path -Path $local_rpgs_path -PathType container))
{
	Write-Error "local-rpgs-path.txt does not point to your local RPG Sounds installation."
	Exit -1
}

# Double-check that the root RPG Sounds folder contains its own RPG Sounds folder as expected
$rpgs_rpgs_path = Join-Path $local_rpgs_path "RPG Sounds"
if (-not (Test-Path -Path $rpgs_rpgs_path -PathType container))
{
	Write-Error "RPG Sounds folder does not conform to expected directory structure.  Double-check your installation."
	Exit -1
}

# Create Mods and community patch folders if they don't already don't exist
$mods_folder_path = Join-Path $local_rpgs_path "RPG Sounds" "Mods"
if (-not (Test-Path -Path $mods_folder_path -PathType container))
{
	Write-Output "Mods folder doesn't exist; creating it now."
	New-Item -Path $rpgs_rpgs_path -Name "Mods" -ItemType directory
}
$patch_mod_folder_path = Join-Path $mods_folder_path "rpgs-community-patch"
if (-not (Test-Path $patch_mod_folder_path -PathType container))
{
	Write-Output "Community Patch folder doesn't exist; creating it now."
	New-Item -Path $mods_folder_path -Name "rpgs-community-patch" -ItemType directory
}

# Copy required files to the patch mod folder
foreach ($file in $files_to_copy)
{
	Copy-Item $file -Destination $patch_mod_folder_path
}

Write-Output "RPG Sounds Community Patch successfully installed!"
Exit 0