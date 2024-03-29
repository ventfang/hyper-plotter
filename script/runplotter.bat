@echo off
setlocal enabledelayedexpansion

set plot_id=1234567890
set drivers=D:\ E:\ F:\ G:\ H:\ I:\ J:\ K:\ L:\ M:\ N:\ O:\

set start_nonce=auto
set nonces=auto
set memory=auto

echo %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
echo Prog Directory: %~dp0hyper-plotter.exe
echo start nonce: %start_nonce%
echo total nonce: %nonces%
echo plot id: %plot_id%
echo drivers: %drivers%
echo %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
cd %~dp0
hyper-plotter.exe -p --id !plot_id! --sn !start_nonce! --num !nonces! --mem !memory! !drivers!

:: check plotting status
for %%a in (!drivers!) do (
  for /f %%b in ('dir /s /b %%a!plot_id!_*.plotting 2^>^nul') do (
    if exist %%b (
      goto unfinished
    )
  )
)

:finished
echo                   @@
echo                  @@@@
echo                 @@@@@
echo                 @@@@@@
echo                @@@@@@@
echo                @@@@@@@@
echo               @@@@@@@@@                  @@@@@@@@@@@@
echo              @@@@@@@@@@                  @@@@@@@@@@@@
echo              @@@@@@@@@                   @@@@@@@@@@@@
echo              @@@@@@@@@                   @@@@@@@@@@@@
echo             @@@@@@@@@   @@                   @@@@                  @@@@@@@                                 @@@@@@@
echo            @@@@@@@@@   @@@@                  @@@@                @@@@@@@@@@@     @@@@@@@@@  @@@@@@@@     @@@@@@@@@@@@
echo            @@@@@@@@@  @@@@@                  @@@@                @@@@@@@@@@@@    @@@@@@@@@  @@@@@@@@     @@@@@@@@@@@@
echo           @@@@@@@@@@  @@@@@                  @@@@                @@@@@  @@@@@    @@@@@@@@   @@@@@@@@     @@@@@@ @@@@@@
echo          @@@@@@@@@@  @@@@@@@                 @@@@                @@      @@@@@     @@@@@     @@@@@       @@@     @@@@@
echo          @@@@@@@@@@  @@@@@@@                 @@@@                        @@@@@      @@@@     @@@@@               @@@@@
echo         @@@@@@@@@   @@@@@@@@@@               @@@@       @@@      @@@@@@@@@@@@@       @@@@    @@@@        @@@@@@@@@@@@@
echo        @@@@@@@@@@    @@@@@@@@@               @@@@       @@@@    @@@@@@@@@@@@@@       @@@@    @@@@       @@@@@@@@@@@@@@
echo        @@@@@@@@@     @@@@@@@@@@              @@@@       @@@@   @@@@@@@@@@@@@@@       @@@@@  @@@@       @@@@@@@@@@@@@@@
echo        @@@@@@@@@      @@@@@@@@@              @@@@       @@@@   @@@@@     @@@@@        @@@@  @@@@       @@@@@      @@@@
echo       @@@@@@@@@       @@@@@@@@@@             @@@@       @@@@   @@@@      @@@@@        @@@@@@@@@        @@@@      @@@@@
echo      @@@@@@@@@         @@@@@@@@@@            @@@@       @@@@   @@@@     @@@@@@         @@@@@@@@        @@@@     @@@@@@
echo      @@@@@@@@           @@@@@@@@@        @@@@@@@@@@@@@@@@@@@   @@@@@@@@@@@@@@@@@       @@@@@@@         @@@@@@@@@@@@@@@@@
echo     @@@@@@@@@            @@@@@@@@        @@@@@@@@@@@@@@@@@@@   @@@@@@@@@@@@@@@@@       @@@@@@@         @@@@@@@@@@@@@@@@@
echo     @@@@@@@@@            @@@@@@@@@       @@@@@@@@@@@@@@@@@@@    @@@@@@@@@@@@@@@@        @@@@@           @@@@@@@@@@@@@@@@
echo    @@@@@@@@@@@@@@@  @@@@@@@@@@@@@@       @@@@@@@@@@@@@@@@@@@     @@@@@@@@ @@@@@@        @@@@@            @@@@@@@@ @@@@@@
echo   @@@@@@@@@@@@@@@@  @@@@@@@@@@@@@@@                               @@@@@                  @@@@              @@@@@
echo  @@@@@@@@@@@@@@@@   @@@@@@@@@@@@@@@@
echo  @@@@@@@@@@@@@@@   @@@@@@@@@@@@@@@@@@
echo @@@@@@@@@@@@@@@@  @@@@@@@@@@@@@@@@@@@
echo @@@@@@@@@@@@@@@   @@@@@@@@@@@@@@@@@@@@
echo @@@@@@@@@@@@@@   @@@@@@@@@@@@@@@@@@@@@
goto done

:unfinished                                                                                                              
echo XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
echo                                       Failed
echo XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
:done
pause