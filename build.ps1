param(
    [switch]$Run
    )

rm -r .\obj\* -ErrorAction SilentlyContinue
rm -r .\bin\* -ErrorAction SilentlyContinue
mkdir obj -ErrorAction SilentlyContinue
mkdir bin -ErrorAction SilentlyContinue

cl /TC .\src\*.c /I .\src\ /W4 /WX /Z7 /nologo /Fo:.\obj\ /Fe:.\bin\slide.exe

if (!$?)
{
    Write-Warning "Failed to compile!"
    return;
}

if ($Run)
{
    Write-Host -ForegroundColor Cyan "`nRunning:"
    .\bin\slide.exe
}