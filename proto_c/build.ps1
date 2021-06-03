param(
    [switch]$Run
    )

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