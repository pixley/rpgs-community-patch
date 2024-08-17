# PowerShell Core 7 or higher is required
if ($PSVersionTable.PSVersion.Major -lt 7)
{
    Write-Error "This script requires PowerShell version 7 or later."
    Exit 1
}

mkdir .\fmod_win32_mf\lib

$auth_token = $Env:TOKEN

$accept_raw_param = "application/vnd.github.raw+json"
$accept_lfs_param = "application/vnd.git-lfs+json"
$authorization_param = "Bearer $auth_token"
$content_type_param = "application/json"
$version_param = "2022-11-28"
$headers_one = @{"Accept"=$accept_raw_param;"X-GitHub-Api-Version"=$version_param;"Authorization"=$authorization_param}
$response_one = Invoke-WebRequest -Headers $headers_one -Uri https://api.github.com/repos/pixley/fmod-lib-for-rpgs-patch/contents/fmod_vc.lib

if (-not $response_one.BaseResponse.IsSuccessStatusCode)
{
	$status_code = $response_one.BaseResponse.StatusCode
	Write-Error "Could not get hash from GitHub.  Status code $status_code."
	Exit 1
}

$response_one.Content > .\fmod_win32_mf\lib\fmod_vc.lib

Exit 0