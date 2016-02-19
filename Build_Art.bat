@echo off
git rev-parse HEAD > shaPrev.txt
set /p shaPrev=<shaPrev.txt
git fetch origin
git reset --hard origin/bbs
git rev-parse HEAD > shaCurrent.txt
set /p shaCurrent=<shaCurrent.txt

IF %shaPrev%==%shaCurrent% (
	echo "Performing minimal rebuild"
	call cd..
	call _build_art_minimal.bat
) ELSE (
	echo "Performing full rebuild"
	call cd..
	call _build_art.bat
)
