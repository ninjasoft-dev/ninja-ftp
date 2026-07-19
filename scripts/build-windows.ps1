<#
.SYNOPSIS
Compila e prepara uma versão portátil do FileZilla no MSYS2 UCRT64.

.DESCRIPTION
Valida as dependências, reutiliza o build incremental e reúne as DLLs ao lado
do executável. A extensão do Explorer é omitida porque não é necessária para
desenvolver ou avaliar a interface.
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 64)]
    [int]$Jobs = 4,

    [switch]$Reconfigure,

    [switch]$Run
)

$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }
$bashPath = Join-Path $msysRoot "usr\bin\bash.exe"
$pacmanPath = Join-Path $msysRoot "usr\bin\pacman.exe"

$requiredPackages = @(
    "base-devel",
    "mingw-w64-ucrt-x86_64-autotools",
    "mingw-w64-ucrt-x86_64-boost",
    "mingw-w64-ucrt-x86_64-cppunit",
    "mingw-w64-ucrt-x86_64-fzssh",
    "mingw-w64-ucrt-x86_64-gcc",
    "mingw-w64-ucrt-x86_64-gettext-tools",
    "mingw-w64-ucrt-x86_64-gnutls",
    "mingw-w64-ucrt-x86_64-libfilezilla",
    "mingw-w64-ucrt-x86_64-pkgconf",
    "mingw-w64-ucrt-x86_64-sqlite3",
    "mingw-w64-ucrt-x86_64-wxwidgets3.2-msw"
)

if (-not (Test-Path -LiteralPath $bashPath)) {
    throw "MSYS2 não encontrado em $msysRoot. Consulte BUILDING_WINDOWS.md."
}

$missingPackages = foreach ($packageName in $requiredPackages) {
    & $pacmanPath -Q $packageName *> $null
    if ($LASTEXITCODE -ne 0) {
        $packageName
    }
}

if ($missingPackages) {
    $packageList = $missingPackages -join " "
    throw "Dependências ausentes no MSYS2: $packageList"
}

$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"
$env:MSYS2_PATH_TYPE = "minimal"

$msysProjectRoot = (& $bashPath -lc "cygpath -u '$projectRoot'").Trim()
if ($LASTEXITCODE -ne 0 -or -not $msysProjectRoot) {
    throw "Não foi possível converter o caminho do projeto para o MSYS2."
}
if ($msysProjectRoot.Contains("'")) {
    throw "O caminho do projeto contém apóstrofo e não pode ser usado pelo script."
}

function Invoke-UcrtBash {
    <# Executa um comando no ambiente UCRT64 e interrompe ao primeiro erro. #>
    param([Parameter(Mandatory)][string]$Command)

    & $script:bashPath -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Comando do MSYS2 falhou com código $LASTEXITCODE."
    }
}

function Repair-LibtoolCxxRuntimeDetection {
    <#
    Remove o uso incompleto de -nostdlib gerado por versões do Libtool que não
    conseguem detectar as bibliotecas padrão do GCC 16 no MinGW.
    #>
    param([Parameter(Mandatory)][string]$LibtoolPath)

    $content = [IO.File]::ReadAllText($LibtoolPath)
    $startMarker = "# ### BEGIN LIBTOOL TAG CONFIG: CXX"
    $endMarker = "# ### END LIBTOOL TAG CONFIG: CXX"
    $start = $content.IndexOf($startMarker, [StringComparison]::Ordinal)
    $end = $content.IndexOf($endMarker, $start, [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -lt 0) {
        throw "Seção CXX não encontrada no Libtool: $LibtoolPath"
    }

    $sectionLength = $end - $start
    $section = $content.Substring($start, $sectionLength)
    if ($section.Contains('postdeps=""') -and $section.Contains(" -nostdlib")) {
        $section = $section.Replace(" -nostdlib", "")
        $content = $content.Substring(0, $start) + $section +
            $content.Substring($end)
        $utf8WithoutBom = [Text.UTF8Encoding]::new($false)
        [IO.File]::WriteAllText($LibtoolPath, $content, $utf8WithoutBom)
        Write-Host "Compatibilidade do Libtool com o GCC corrigida." `
            -ForegroundColor Yellow
    }
}

$buildDirectory = Join-Path $projectRoot "build-ucrt64"
$configStatus = Join-Path $buildDirectory "config.status"
$configHeader = Join-Path $buildDirectory "config\config.h"
$libtoolPath = Join-Path $buildDirectory "libtool"

if ($Reconfigure -or -not (Test-Path -LiteralPath $configStatus)) {
    Write-Host "Configurando o build UCRT64..." -ForegroundColor Cyan
    $configureCommand = @"
cd '$msysProjectRoot'
mkdir -p build-ucrt64
cd build-ucrt64
CFLAGS='-Wno-incompatible-pointer-types' CXXFLAGS='-Wno-incompatible-pointer-types' ../configure -C --prefix=/ucrt64 --build=`$MINGW_CHOST --host=`$MINGW_CHOST --target=`$MINGW_CHOST --disable-shellext --disable-manualupdatecheck --disable-autoupdatecheck --with-pugixml=builtin --with-wx-config=/ucrt64/bin/wx-config-3.2
"@
    Invoke-UcrtBash $configureCommand
}

if (-not (Test-Path -LiteralPath $configHeader)) {
    Write-Host "Finalizando os arquivos de configuração..." -ForegroundColor Cyan
    Invoke-UcrtBash "cd '$msysProjectRoot/build-ucrt64'; ./config.status"
}

if (-not (Test-Path -LiteralPath $libtoolPath)) {
    throw "Libtool não encontrado depois da configuração: $libtoolPath"
}
Repair-LibtoolCxxRuntimeDetection -LibtoolPath $libtoolPath

Write-Host "Compilando com $Jobs tarefas paralelas..." -ForegroundColor Cyan
Invoke-UcrtBash "cd '$msysProjectRoot/build-ucrt64'; make MAYBE_FZSHELLEXT= -j$Jobs"

Write-Host "Preparando a pasta portátil..." -ForegroundColor Cyan
$stagePath = "$msysProjectRoot/build-ucrt64/stage"
Invoke-UcrtBash "cd '$msysProjectRoot/build-ucrt64'; make MAYBE_FZSHELLEXT= install DESTDIR='$stagePath'"

$portableBin = Join-Path $buildDirectory "stage\ucrt64\bin"
$dependencyDirectory = Join-Path $buildDirectory "data\dlls_gui"
Copy-Item -Path (Join-Path $dependencyDirectory "*.dll") -Destination $portableBin -Force

$libfilezillaCatalog = Join-Path $msysRoot "ucrt64\share\locale\pt_BR\LC_MESSAGES\libfilezilla.mo"
$portableLocaleDirectory = Join-Path $buildDirectory "stage\ucrt64\share\locale\pt_BR\LC_MESSAGES"
if (-not (Test-Path -LiteralPath $libfilezillaCatalog)) {
    throw "Catálogo pt-BR da libfilezilla não encontrado: $libfilezillaCatalog"
}
New-Item -ItemType Directory -Path $portableLocaleDirectory -Force | Out-Null
Copy-Item -LiteralPath $libfilezillaCatalog -Destination $portableLocaleDirectory -Force

$executablePath = Join-Path $portableBin "filezilla.exe"
if (-not (Test-Path -LiteralPath $executablePath)) {
    throw "O executável não foi encontrado depois da compilação."
}

Write-Host "Build concluído: $executablePath" -ForegroundColor Green

if ($Run) {
    Start-Process -FilePath $executablePath -WorkingDirectory (Split-Path $portableBin)
}
