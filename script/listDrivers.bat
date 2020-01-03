@echo off
setlocal enabledelayedexpansion
set "drivers= "
for /f "skip=2 delims= " %%a in ('wmic logicaldisk get Caption') do (
  set drivers=!drivers!%%a 
)
echo !drivers!