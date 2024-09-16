@echo off

REM the directory path of this file
set p=%~dp0

REM Unicode version
set UVER=16.0.0

for %%f in (IdnaTestV2.txt) do (
  curl -fsS -o %p%\data\%%f https://www.unicode.org/Public/idna/%UVER%/%%f
)

for %%f in (NormalizationTest.txt) do (
  curl -fsS -o %p%\data\%%f https://www.unicode.org/Public/%UVER%/ucd/%%f
)
