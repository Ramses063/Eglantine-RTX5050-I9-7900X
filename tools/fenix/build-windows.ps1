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
#          -Branch <name> (default fenix), -NoPull (build what is checked out), -Run (start it),
#          -NoLTO (skip whole-program optimization: quicker links, a bit slower Fenix),
#          -Train (profile-guided optimization: builds a measuring Fenix, you play a few minutes
#          and close it, then Fenix is rebuilt around what it measured; later builds keep using
#          that profile).
# The finished Fenix is copied to the -Install folder (it is also in <Source>\build\bin).

param(
    [switch]$Setup,
    [string]$Source = "C:\dev\fenix",
    [string]$Install = "E:\Games\Nintendo Switch\Fenix",
    [string]$Branch = "fenix",
    [switch]$NoPull,
    [switch]$Run,
    [switch]$NoLTO,
    [switch]$Train
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

# A newer copy of this script came with the source: let it do the build.
$repoScript = Join-Path $Source "tools\fenix\build-windows.ps1"
if (-not $env:FENIX_BUILD_CHILD -and (Test-Path $repoScript) -and
    (Get-FileHash $repoScript).Hash -ne (Get-FileHash $PSCommandPath).Hash) {
    Copy-Item $repoScript $PSCommandPath -Force -ErrorAction SilentlyContinue
    $env:FENIX_BUILD_CHILD = "1"
    & $repoScript -Source $Source -Install $Install -Branch $Branch -NoPull -Run:$Run -NoLTO:$NoLTO -Train:$Train
    exit $LASTEXITCODE
}

Set-Location $Source
$build = Join-Path $Source "build"
# This PC's CPU: AVX2, no Intel-only padding, and whole-program optimization (-NoLTO for quicker
# links while testing).
$bin = Join-Path $build "bin"
$pgd = Join-Path $bin "Fenix.pgd"
$trained = Join-Path $bin "Fenix.trained"
# Built with clang-cl, ThinLTO and the source's profile
# trained on Ocarina of Time (lichii-windows.profdata). The older Fenix builds with MSVC.
$useClang = Test-Path (Join-Path $Source "lichii-windows.profdata")
$LlvmVersion = "22.1.8"
$LlvmRoot = "C:\dev\llvm-$LlvmVersion"
if ($useClang -and -not (Test-Path (Join-Path $LlvmRoot "bin\clang-cl.exe"))) {
    Step "Installing LLVM $LlvmVersion (the compiler Fenix builds with; once)"
    $installer = Join-Path $env:TEMP "LLVM-$LlvmVersion-win64.exe"
    Invoke-WebRequest -OutFile $installer `
        "https://github.com/llvm/llvm-project/releases/download/llvmorg-$LlvmVersion/LLVM-$LlvmVersion-win64.exe"
    if ((Get-FileHash -Algorithm SHA256 $installer).Hash -ne
        "16E5709785FEF73C854646241C4A92C5CD574318D1B33C63330DD7721903E55C") {
        throw "The LLVM download did not match its checksum. Run this again."
    }
    Write-Host "Windows may ask to allow the LLVM installer: click Yes." -ForegroundColor Yellow
    Start-Process -Wait -FilePath $installer -ArgumentList "/S", "/D=$LlvmRoot"
    if (-not (Test-Path (Join-Path $LlvmRoot "bin\clang-cl.exe"))) {
        throw "LLVM did not install to $LlvmRoot."
    }
}
function Get-FastFlags($pgo) {
    if ($useClang) {
        $llvm = $LlvmRoot.Replace("\", "/")
        $profdata = (Join-Path $Source "lichii-windows.profdata").Replace("\", "/")
        # _USE_STD_VECTOR_ALGORITHMS=0: Visual Studio 2026's library (14.5x) static_asserts in its
        # vectorized std::find for some types under clang-cl.
        $c = "-DWIN32 -D_WINDOWS -DNOMINMAX -D_USE_STD_VECTOR_ALGORITHMS=0 /arch:AVX2"
        if (-not $NoLTO) {
            $c += " -flto=thin -fprofile-use=$profdata -Wno-error=profile-instr-out-of-date" +
                  " -Wno-error=profile-instr-unprofiled -Wno-error=backend-plugin"
        }
        return @("-DCMAKE_C_COMPILER=$llvm/bin/clang-cl.exe", "-DCMAKE_CXX_COMPILER=$llvm/bin/clang-cl.exe",
                 "-DCMAKE_LINKER=$llvm/bin/lld-link.exe", "-DCMAKE_AR=$llvm/bin/llvm-lib.exe",
                 "-DCMAKE_C_FLAGS=$c", "-DCMAKE_CXX_FLAGS=-EHsc $c")
    }
    @("-DFENIX_AVX2=ON", "-DFENIX_NO_JCC_PADDING=ON",
      "-DFENIX_LTO=$(if ($NoLTO) { 'OFF' } else { 'ON' })", "-DFENIX_PGO=$pgo")
}
# A build folder made by the other compiler can't be reused.
$cache = Join-Path $build "CMakeCache.txt"
if (Test-Path $cache) {
    $clangCache = [bool](Select-String -Path $cache -Pattern "clang-cl" -SimpleMatch -Quiet)
    if ($clangCache -ne $useClang) {
        Step "Starting the build folder over (compiler changed)"
        Remove-Item -Recurse -Force $build
    }
}
function Invoke-Configure($pgo) {
    $flags = Get-FastFlags $pgo
    if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
        Step "Configuring (first time only)"
        cmake -S . -B $build -G Ninja `
            -DCMAKE_BUILD_TYPE=Release `
            "-DCMAKE_PREFIX_PATH=$QtDir" `
            -DBUILD_TESTING=OFF -DYUZU_TESTS=OFF -DYUZU_CMD=ON `
            -DYUZU_ROOM=OFF -DYUZU_ROOM_STANDALONE=OFF `
            -DYUZU_USE_BUNDLED_QT=OFF -DYUZU_USE_BUNDLED_SDL3=ON `
            -DENABLE_QT_TRANSLATION=ON -DENABLE_UPDATE_CHECKER=OFF -DUSE_DISCORD_PRESENCE=ON `
            @flags | Out-Host
    } else {
        # Quick refresh so the version Fenix shows (and the debug report) is this commit.
        cmake -S . -B $build @flags | Out-Null
    }
    if ($LASTEXITCODE -ne 0) { throw "Configure failed (run it again; send the error if it repeats)." }
}
function Invoke-Build($targets) {
    cmake --build $build --target @targets --parallel | Out-Host
    return $LASTEXITCODE -eq 0
}

$started = Get-Date
if ($Train -and $useClang) {
    Write-Host "This Fenix already uses a profile trained on Ocarina of Time: no training needed." -ForegroundColor Yellow
    $Train = $false
}
if ($Train -and -not $NoLTO) {
    Step "Building the measuring Fenix (profile-guided optimization, step 1 of 2)"
    Remove-Item $trained -ErrorAction SilentlyContinue
    Get-ChildItem $bin -Filter "*.pgc" -ErrorAction SilentlyContinue | Remove-Item -Force
    Remove-Item $pgd -ErrorAction SilentlyContinue
    Invoke-Configure "GENERATE"
    if (-not (Invoke-Build @("yuzu"))) { throw "Build failed: copy the first 'error' lines above and send them." }
    windeployqt --release --no-translations (Join-Path $bin "Fenix.exe") | Out-Null
    $pgort = Join-Path $env:VCToolsInstallDir "bin\Hostx64\x64\pgort140.dll"
    if (Test-Path $pgort) { Copy-Item $pgort $bin -Force }
    Write-Host "`nFenix opens now. It runs SLOW on purpose: it is measuring." -ForegroundColor Yellow
    Write-Host "Play 5 minutes in the places that run worst, then CLOSE Fenix (File > Exit)." -ForegroundColor Yellow
    Write-Host "This window continues by itself after you close it.`n" -ForegroundColor Yellow
    $env:VCPROFILE_PATH = $bin
    Start-Process (Join-Path $bin "Fenix.exe") -WorkingDirectory $bin -Wait
    $counts = Get-ChildItem $bin -Filter "*.pgc" -ErrorAction SilentlyContinue
    if (-not $counts) {
        Write-Host "No measurements were written (Fenix has to be closed normally). Building without them." -ForegroundColor Yellow
        Remove-Item $pgd -ErrorAction SilentlyContinue
    } else {
        Set-Content -Path $trained -Value (Get-Date).ToString("s")
    }
}

$pgo = if (-not $NoLTO -and (Test-Path $trained) -and (Test-Path $pgd)) { "USE" } else { "OFF" }
Step "Building (all CPU threads)$(if ($pgo -eq 'USE') { ', using your play profile' })"
Invoke-Configure $pgo
if (-not (Invoke-Build @("yuzu", "yuzu-cmd"))) {
    $built = $false
    if ($pgo -eq "USE") {
        # A profile from older code can stop the link: build without it (run -Train again later).
        Write-Host "The play profile no longer fits this code: building without it." -ForegroundColor Yellow
        Remove-Item $trained -ErrorAction SilentlyContinue
        Invoke-Configure "OFF"
        $built = Invoke-Build @("yuzu", "yuzu-cmd")
    }
    if (-not $built -and -not $NoLTO) {
        # The compiler's whole-program step can fail on its own saved state: clear it, try once.
        Write-Host "Trying again with the linker's saved work cleared..." -ForegroundColor Yellow
        foreach ($pattern in "*.iobj", "*.ipdb") {
            Get-ChildItem -Path $bin -Filter $pattern -File -ErrorAction SilentlyContinue |
                Remove-Item -Force -ErrorAction SilentlyContinue
        }
        $built = Invoke-Build @("yuzu", "yuzu-cmd")
    }
    if (-not $built -and -not $NoLTO) {
        # Still failing: build without whole-program optimization (a bit slower, but it runs).
        Write-Host "Building without whole-program optimization this time." -ForegroundColor Yellow
        $NoLTO = $true
        Invoke-Configure "OFF"
        $built = Invoke-Build @("yuzu", "yuzu-cmd")
    }
    if (-not $built) { throw "Build failed: copy the first 'error' lines above and send them." }
}

Step "Adding the Qt files Fenix needs"
windeployqt --release --no-translations (Join-Path $bin "Fenix.exe") | Out-Null

$exe = Join-Path $bin "Fenix.exe"
if ($Install) {
    # Copied over the previous version; a "user" folder there (portable saves) is never touched.
    Step "Copying Fenix to $Install"
    New-Item -ItemType Directory -Force -Path $Install | Out-Null
    # Not the linker's work files (hundreds of MiB) or the profile.
    $skip = @("*.iobj", "*.ipdb", "*.pgd", "*.pgc", "pgort140.dll", "Fenix.trained")
    robocopy $bin $Install /E /XD user /XF @skip /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copying to $Install failed (is Fenix still open there? Close it and run again)." }
    foreach ($pattern in "*.iobj", "*.ipdb", "*.pgd", "*.pgc") {
        Get-ChildItem -Path $Install -Filter $pattern -File -ErrorAction SilentlyContinue |
            Remove-Item -Force -ErrorAction SilentlyContinue
    }
    $exe = Join-Path $Install "Fenix.exe"
}

$minutes = [math]::Round(((Get-Date) - $started).TotalMinutes, 1)
Write-Host "`nDone in $minutes min: $exe" -ForegroundColor Green
if ($Run) { Start-Process $exe -WorkingDirectory (Split-Path $exe) }
exit 0
