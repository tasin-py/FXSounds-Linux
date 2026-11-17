# Build fxprocess.exe on Windows with MSVC from the PRISTINE upstream sources (no Linux changes),
# as the reference for null tests against the Linux port.
#   powershell -File tools\build-windows-reference.ps1 [-Upstream reference\upstream-pristine]
param([string]$Upstream = "$PSScriptRoot\..\reference\upstream-pristine")
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..").Path
$up = (Resolve-Path $Upstream).Path
$out = Join-Path $root 'build-win'
New-Item -ItemType Directory -Force "$out\obj" | Out-Null

$d = "$up\dsp"; $a = "$up\audiopassthru\src"
$srcs = @()
$srcs += ([xml](Get-Content "$d\DfxDsp.vcxproj")).Project.ItemGroup.ClCompile.Include | Where-Object { $_ } | ForEach-Object { Join-Path $d $_ }
foreach ($m in 'MRY','MTH','pstr','FILE','ptime','reg','SLOUT','operatingSystem') { $srcs += Get-ChildItem "$a\$m\*.cpp" | ForEach-Object FullName }
$srcs = $srcs | Select-Object -Unique

$common = @('/nologo','/O2','/MT','/EHsc','/w','/DWIN32','/DNDEBUG','/D_LIB','/DPT_NON_MFC','/DDSPSOFT_TARGET',
            '/DPT_DSP_BUILD=PT_DSP_DFX','/DUNICODE','/D_UNICODE','/D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING')
$rsp1 = $common + @('/c', "/I`"$d\ptutil\include`"", "/I`"$d\include`"", "/I`"$d`"", "/I`"$up\audiopassthru\include`"", "/Fo`"$out\obj\\`"") + ($srcs | ForEach-Object { "`"$_`"" })
$rsp1 | Set-Content -Encoding ascii "$out\compile.rsp"
$rsp2 = @('/nologo','/O2','/MT','/EHsc','/std:c++17','/D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING', "/I`"$d\include`"",
          "`"$root\tools\fxprocess.cpp`"", "`"$out\fxdsp.lib`"", "/Fe`"$out\fxprocess.exe`"", "/Fo`"$out\\`"",
          'advapi32.lib','ole32.lib','user32.lib','shlwapi.lib','winmm.lib','shell32.lib','version.lib')
$rsp2 | Set-Content -Encoding ascii "$out\link.rsp"

$vsdev = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat'
$bat = "@echo off`r`ncall `"$vsdev`" -arch=x64 -host_arch=x64 >nul`r`ncl @`"$out\compile.rsp`" || exit /b 1`r`nlib /nologo /OUT:`"$out\fxdsp.lib`" `"$out\obj\*.obj`" || exit /b 1`r`ncl @`"$out\link.rsp`" || exit /b 1`r`n"
Set-Content -Encoding ascii "$out\build.bat" $bat
$ErrorActionPreference = 'Continue'
cmd /c "`"$out\build.bat`" > `"$out\build.log`" 2>&1"
$rc = $LASTEXITCODE
Get-Content "$out\build.log" | Where-Object { $_ -match ' error |fatal error' } | Select-Object -First 30
if ($rc -ne 0) { throw "MSVC build failed ($rc), see $out\build.log" }
"built $out\fxprocess.exe"
