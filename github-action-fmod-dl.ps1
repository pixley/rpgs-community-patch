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

$response_one_str = [System.Text.Encoding]::UTF8.GetString($response_one.Content)
$response_one_str = $response_one_str.Replace(' ', '=')
$response_one_dict = ConvertFrom-StringData -StringData $response_one_str

$dl_request = @{operation="download";transfers=,"basic";objects=,@{oid=$response_one_dict["oid"].Substring(7);size=[int]$response_one_dict["size"]}}
$dl_request_json = $dl_request | ConvertTo-Json -Depth 4

$headers_two = @{"Accept"=$accept_lfs_param;"X-GitHub-Api-Version"=$version_param;"Content-Type"=$content_type_param;"Authorization"=$authorization_param}
$response_two = Invoke-RestMethod -Method 'Post' -Headers $headers_two -Body $dl_request_json -Uri https://github.com/pixley/fmod-lib-for-rpgs-patch.git/info/lfs/objects/batch

if (($response_two.objects.Count -eq 0) -or ($response_two.objects.Get(0).actions -eq $null))
{
	Write-Error "Did not receive valid download action."
	Exit 1
}

Invoke-WebRequest -Uri $response_two.objects.Get(0).actions.download.href -OutFile .\fmod_win32_mf\lib\fmod_vc.dll

Exit 0