[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repositoryRoot "out-openxr"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Visual Studio Installer was not found. Install Visual Studio with Desktop development with C++."
}

$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) {
    throw "A Visual Studio C++ toolchain was not found. Install Desktop development with C++."
}

$developerPrompt = Join-Path $visualStudio "Common7\Tools\VsDevCmd.bat"
$configure = "cmake -S `"$repositoryRoot`" -B `"$buildDirectory`" -G Ninja -DCMAKE_BUILD_TYPE=$Configuration"
$build = "cmake --build `"$buildDirectory`" --config $Configuration"
$command = "call `"$developerPrompt`" -arch=x64 -host_arch=x64 && $configure && $build"

& cmd.exe /d /s /c $command
if ($LASTEXITCODE -ne 0) {
    throw "The Pixel Flow XR build failed with exit code $LASTEXITCODE."
}

$executable = Join-Path $buildDirectory "bin\PixelFlowXR.exe"
Write-Host "Built $executable"
