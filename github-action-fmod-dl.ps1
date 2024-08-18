# ===================================
# This script is specific to the GitHub Action that builds releases.
# This has no use for manual development.
# ===================================

# PowerShell Core 7 or higher is required
if ($PSVersionTable.PSVersion.Major -lt 7)
{
    Write-Error "This script requires PowerShell version 7 or later."
    Exit 1
}

# Need to make the directory first
mkdir .\fmod_win32_mf\lib

# TOKEN envvar set via the Action YAML script
$auth_token = $Env:TOKEN

$accept_raw_param = "application/vnd.github.raw+json"
$authorization_param = "Bearer $auth_token"
$version_param = "2022-11-28"
$headers = @{"Accept"=$accept_raw_param;"X-GitHub-Api-Version"=$version_param;"Authorization"=$authorization_param}
$response = Invoke-WebRequest -Headers $headers -Uri https://api.github.com/repos/pixley/fmod-lib-for-rpgs-patch/contents/fmod_vc.lib

if (-not $response.BaseResponse.IsSuccessStatusCode)
{
	$status_code = $response.BaseResponse.StatusCode
	Write-Error "Failure response from GitHub.  Status code $status_code."
	Exit 1
}

Set-Content .\fmod_win32_mf\lib\fmod_vc.lib -Value $response.Content -Encoding Byte

Exit 0