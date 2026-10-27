param(
    [string]$Output = "bin\switchc.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = (Resolve-Path -LiteralPath $root).Path

$wsl = Get-Command wsl -ErrorAction SilentlyContinue
if (-not $wsl) {
    throw "WSL is required to run Flex and Bison. Install WSL or run 'make' inside WSL."
}

$wslRoot = (& wsl wslpath -a "$root").Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to translate the project path for WSL."
}

& wsl bash -lc "cd '$wslRoot' && bison -d -o src/parser.tab.c src/parser.y && flex -o src/lex.yy.c src/lexer.l"
if ($LASTEXITCODE -ne 0) {
    throw "Flex/Bison generation failed with exit code $LASTEXITCODE"
}

$outputPath = Join-Path $root $Output
$outputDir = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$sources = @(
    "src\parser.tab.c",
    "src\lex.yy.c",
    "src\ast.c",
    "src\semantic.c",
    "src\support.c",
    "src\json_output.c",
    "src\main.c"
) | ForEach-Object { Join-Path $root $_ }

$arguments = @("-std=c11", "-O2", "-Wall", "-Wextra", "-Isrc") + $sources + @("-o", $outputPath)
& gcc @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Compilation failed with exit code $LASTEXITCODE"
}

$sqlcPath = Join-Path $outputDir "sqlc.exe"
Copy-Item -Path $outputPath -Destination $sqlcPath -Force

Write-Output "Generated parser.tab.c, parser.tab.h and lex.yy.c with WSL Flex/Bison"
Write-Output "Built $outputPath and $sqlcPath"
