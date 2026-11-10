# SPDX-FileCopyrightText: Copyright 2026 Fenix contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Builds Fenix on this PC: the same MSVC build as the GitHub one, in minutes after the first time.
#
#   First time (PowerShell as administrator, ~20-30 min of installs):
#       powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Setup
#   Every build after that (normal PowerShell):
#       powershell -ExecutionPolicy Bypass -File build-windows.ps1
#
# Options: -Source <folder> (default C:\dev\fenix; a short path, MSVC trips over long ones),
#          -Install <folder> (default E:\Games\Nintendo Switch\Fenix; "" to skip the copy),
#          -Branch <name> (default fenix), -NoPull (build what is checked out), -Run (start it).
# The finished Fenix is copied to the -Install folder (it is also in <Source>\build\bin).

param(
    [switch]$Setup,
    [string]$Source = "C:\dev\fenix",
    [string]$Install = "E:\Games\Nintendo Switch\Fenix",
    [string]$Branch = "fenix",
    [switch]$NoPull,
    [switch]$Run
)

$ErrorActionPreference = "Stop"
$Repo = "https://github.com/Ramses063/Eglantine-RTX5050-I9-7900X.git"
$QtVersion = "6.8.3"
$QtRoot = "C:\Qt"
$QtDir = Join-Path $QtRoot "$QtVersion\msvc2022_64"

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Red }

function Refresh-Path {
    $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
                [Environment]::GetEnvironmentVariable("Path", "User")
}

function Find-VsDevCmd {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $path) { return $null }
    return Join-Path $path "Common7\Tools\VsDevCmd.bat"
}

if ($Setup) {
    $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $admin) { throw "Run -Setup from PowerShell opened as administrator." }

    Step "Installing Git, CMake, Ninja and Python (winget)"
    foreach ($id in "Git.Git", "Kitware.CMake", "Ninja-build.Ninja", "Python.Python.3.12") {
        winget install --id $id -e --accept-source-agreements --accept-package-agreements --silent
    }

    if (-not (Find-VsDevCmd)) {
        # Visual Studio 2026's (MSVC 14.5x), the compiler the GitHub builds use.
        Step "Installing Visual Studio Build Tools with C++ (this one takes a while)"
        winget install --id Microsoft.VisualStudio.BuildTools -e --accept-source-agreements `
            --accept-package-agreements --override `
            "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --wait"
    }
    Refresh-Path

    Step "Installing the Vulkan SDK"
    $vulkanScript = Join-Path $PSScriptRoot "..\windows\install-vulkan-sdk.ps1"
    if (-not (Test-Path $vulkanScript)) {
        $vulkanScript = Join-Path $env:TEMP "install-vulkan-sdk.ps1"
        Invoke-WebRequest -OutFile $vulkanScript `
            "https://raw.githubusercontent.com/Ramses063/Eglantine-RTX5050-I9-7900X/$Branch/tools/windows/install-vulkan-sdk.ps1"
    }
    & $vulkanScript

    Step "Installing Qt $QtVersion (aqtinstall)"
    if (-not (Test-Path $QtDir)) {
        # The py launcher, not "python": that name can open the Microsoft Store instead.
        $usePy = [bool](Get-Command py -ErrorAction SilentlyContinue)
        function Invoke-Python { if ($usePy) { & py -3 @args } else { & python @args } }
        Invoke-Python -m pip install --upgrade aqtinstall
        Invoke-Python -m aqt install-qt windows desktop $QtVersion win64_msvc2022_64 -m qtcharts -O $QtRoot
        if (-not (Test-Path $QtDir)) { throw "Qt did not install to ${QtDir}." }
    }
    Write-Host "`nSetup done. Now run this script again without -Setup, from a normal PowerShell." -ForegroundColor Green
    exit 0
}

Refresh-Path
$devCmd = Find-VsDevCmd
if (-not $devCmd) { throw "Visual Studio C++ Build Tools not found: run this script with -Setup first." }
if (-not (Test-Path $QtDir)) { throw "Qt not found at ${QtDir}: run this script with -Setup first." }
$vulkan = Get-ChildItem "C:\VulkanSDK" -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
if (-not $vulkan) { throw "Vulkan SDK not found: run this script with -Setup first." }

# The compiler's environment (vcvars), copied into this PowerShell session.
Step "Loading the MSVC environment"
cmd /c "`"$devCmd`" -arch=x64 -host_arch=x64 >nul && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
}
$env:VULKAN_SDK = $vulkan.FullName
$env:Path = "$($vulkan.FullName)\Bin;$QtDir\bin;$env:Path"

if (-not (Test-Path (Join-Path $Source ".git"))) {
    Step "Downloading the source to $Source"
    New-Item -ItemType Directory -Force -Path (Split-Path $Source) | Out-Null
    git clone --branch $Branch $Repo $Source
} elseif (-not $NoPull) {
    Step "Getting the latest $Branch"
    git -C $Source fetch origin $Branch
    git -C $Source checkout $Branch
    git -C $Source reset --hard "origin/$Branch"
}

Set-Location $Source
$build = Join-Path $Source "build"
if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
    Step "Configuring (first time only)"
    cmake -S . -B $build -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$QtDir" `
        -DBUILD_TESTING=OFF -DYUZU_TESTS=OFF -DYUZU_CMD=ON `
        -DYUZU_ROOM=OFF -DYUZU_ROOM_STANDALONE=OFF `
        -DYUZU_USE_BUNDLED_QT=OFF -DYUZU_USE_BUNDLED_SDL3=ON `
        -DENABLE_QT_TRANSLATION=ON -DENABLE_UPDATE_CHECKER=OFF -DUSE_DISCORD_PRESENCE=ON `
        -DFENIX_AVX2=ON
    if ($LASTEXITCODE -ne 0) { throw "Configure failed (see above)." }
}

Step "Building (all CPU threads)"
$started = Get-Date
cmake --build $build --target yuzu yuzu-cmd --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed: copy the first 'error' lines above and send them." }

Step "Adding the Qt files Fenix needs"
windeployqt --release --no-translations (Join-Path $build "bin\Fenix.exe") | Out-Null

$exe = Join-Path $build "bin\Fenix.exe"
if ($Install) {
    # Copied over the previous version; a "user" folder there (portable saves) is never touched.
    Step "Copying Fenix to $Install"
    New-Item -ItemType Directory -Force -Path $Install | Out-Null
    robocopy (Join-Path $build "bin") $Install /E /XD user /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copying to $Install failed (is Fenix still open there? Close it and run again)." }
    $exe = Join-Path $Install "Fenix.exe"
}

$minutes = [math]::Round(((Get-Date) - $started).TotalMinutes, 1)
Write-Host "`nDone in $minutes min: $exe" -ForegroundColor Green
if ($Run) { Start-Process $exe -WorkingDirectory (Split-Path $exe) }
