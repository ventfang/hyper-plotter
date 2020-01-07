@echo off
setlocal enabledelayedexpansion

set plot_id=1eccd66cd659b0c665ffdcebfda5f86c60cef300
set drivers=D:\ E:\ F:\ G:\ H:\ I:\ J:\ K:\ L:\ M:\ N:\ O:\

set start_nonce=auto
set nonces=auto
set memory=auto

echo %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
echo Prog Directory: %~dp0parallel-plotter.exe
echo start nonce: %start_nonce%
echo total nonce: %nonces%
echo plot id: %plot_id%
echo drivers: %drivers%
echo %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
parallel-plotter.exe -p --id !plot_id! --sn !start_nonce! --num !nonces! --mem !memory! !drivers!
pause