@echo off
setlocal enabledelayedexpansion

set start_nonce=0
set nonces=-1
set plot_id=a25a99a3d8e5a0cf47134297efe851486c99431f

for /f %%a in ('listDrivers.bat') do ( set drivers=%%a)
parallel-plotter.exe -p --id !plot_id! --sn !start_nonce! --num !nonces! !drivers!