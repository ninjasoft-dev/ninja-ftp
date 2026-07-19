<#
.SYNOPSIS
Gera a distribuição portátil do NinjaSoft FTP para Windows.

.DESCRIPTION
Compila o projeto, copia o runtime necessário, reduz os binários da cópia de
distribuição e cria um ZIP acompanhado de seu checksum SHA-256.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern("^v?\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$")]
    [string]$Version,

    [ValidateRange(1, 64)]
    [int]$Jobs = 4,

    [switch]$Reconfigure,

    [switch]$KeepSymbols
)

$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildScript = Join-Path $PSScriptRoot "build-windows.ps1"
$buildDirectory = Join-Path $projectRoot "build-ucrt64"
$stageRoot = Join-Path $buildDirectory "stage\ucrt64"
$sourceBin = Join-Path $stageRoot "bin"
$sourceResources = Join-Path $stageRoot "share\filezilla"
$sourceLocales = Join-Path $stageRoot "share\locale"
$outputDirectory = Join-Path $projectRoot "dist"
$normalizedVersion = $Version.TrimStart("v")
$packageName = "NinjaSoft-FTP-$normalizedVersion-Windows-x64-portable"
$assemblyDirectory = Join-Path $buildDirectory "package"
$packageRoot = Join-Path $assemblyDirectory $packageName
$archivePath = Join-Path $outputDirectory "$packageName.zip"
$checksumPath = "$archivePath.sha256"

& $buildScript -Jobs $Jobs -Reconfigure:$Reconfigure

foreach ($requiredPath in @($sourceBin, $sourceResources, $sourceLocales)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Arquivo necessário para o pacote não encontrado: $requiredPath"
    }
}

# A versão é validada antes de compor os caminhos, limitando a limpeza à área
# temporária reservada ao empacotamento.
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

$packageBin = Join-Path $packageRoot "bin"
$packageShare = Join-Path $packageRoot "share"
New-Item -ItemType Directory -Path $packageBin -Force | Out-Null
New-Item -ItemType Directory -Path $packageShare -Force | Out-Null

Copy-Item -Path (Join-Path $sourceBin "*") -Destination $packageBin -Recurse -Force
Copy-Item -LiteralPath $sourceResources `
    -Destination (Join-Path $packageShare "filezilla") -Recurse -Force
Copy-Item -LiteralPath $sourceLocales `
    -Destination (Join-Path $packageShare "locale") -Recurse -Force

$originalExecutable = Join-Path $packageBin "filezilla.exe"
$brandedExecutable = Join-Path $packageBin "NinjaSoftFTP.exe"
if (-not (Test-Path -LiteralPath $originalExecutable)) {
    throw "Executável compilado não encontrado: $originalExecutable"
}
Move-Item -LiteralPath $originalExecutable -Destination $brandedExecutable

Copy-Item -LiteralPath (Join-Path $projectRoot "COPYING") `
    -Destination (Join-Path $packageRoot "LICENSE.txt")
Copy-Item -LiteralPath (Join-Path $projectRoot "AUTHORS") `
    -Destination (Join-Path $packageRoot "AUTHORS.txt")
Copy-Item -LiteralPath (Join-Path $projectRoot "README.md") `
    -Destination (Join-Path $packageRoot "README.md")
Copy-Item -LiteralPath (Join-Path $projectRoot "docs\PORTABLE_WINDOWS.md") `
    -Destination (Join-Path $packageRoot "LEIA-ME.md")

if (-not $KeepSymbols) {
    $msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }
    $stripPath = Join-Path $msysRoot "ucrt64\bin\strip.exe"
    if (-not (Test-Path -LiteralPath $stripPath)) {
        throw "Ferramenta strip não encontrada: $stripPath"
    }

    Get-ChildItem -LiteralPath $packageBin -File |
        Where-Object { $_.Extension -in ".exe", ".dll" } |
        ForEach-Object {
            & $stripPath --strip-unneeded $_.FullName
            if ($LASTEXITCODE -ne 0) {
                throw "Não foi possível reduzir o binário: $($_.FullName)"
            }
        }
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
foreach ($oldArtifact in @($archivePath, $checksumPath)) {
    if (Test-Path -LiteralPath $oldArtifact) {
        Remove-Item -LiteralPath $oldArtifact -Force
    }
}

Compress-Archive -LiteralPath $packageRoot -DestinationPath $archivePath `
    -CompressionLevel Optimal

$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
"$archiveHash  $([IO.Path]::GetFileName($archivePath))" |
    Set-Content -LiteralPath $checksumPath -Encoding ascii

Write-Host "Pacote criado: $archivePath" -ForegroundColor Green
Write-Host "SHA-256: $archiveHash" -ForegroundColor Green
