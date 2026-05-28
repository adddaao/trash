param(
    [string]$InstallDir = "$env:USERPROFILE\.local",
    [switch]$NoPath
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir
$xmake = Get-Command xmake -ErrorAction SilentlyContinue
if ($null -eq $xmake) {
    $defaultXmake = "C:\Program Files\xmake\xmake.exe"
    if (Test-Path -LiteralPath $defaultXmake) {
        $xmakePath = $defaultXmake
    } else {
        throw "未找到 xmake，请先安装 xmake，或把 xmake 加入 PATH。"
    }
} else {
    $xmakePath = $xmake.Source
}

Write-Host "项目目录: $projectDir"
Write-Host "安装目录: $InstallDir"
Write-Host "使用 xmake: $xmakePath"

Push-Location $projectDir
try {
    & $xmakePath f -c
    & $xmakePath
    & $xmakePath install -o $InstallDir
} finally {
    Pop-Location
}

$binDir = Join-Path $InstallDir "bin"
if (-not $NoPath) {
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $pathItems = @()
    if (-not [string]::IsNullOrWhiteSpace($currentPath)) {
        $pathItems = $currentPath -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    }

    if ($pathItems -notcontains $binDir) {
        $newPath = if ([string]::IsNullOrWhiteSpace($currentPath)) { $binDir } else { "$currentPath;$binDir" }
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Host "已加入用户 PATH: $binDir"
        Write-Host "请重新打开 PowerShell 后使用 trash 命令。"
    } else {
        Write-Host "用户 PATH 已包含: $binDir"
    }
}

$exePath = Join-Path $binDir "trash.exe"
if (Test-Path -LiteralPath $exePath) {
    Write-Host "安装完成: $exePath"
} else {
    Write-Host "安装完成，请检查: $binDir"
}
