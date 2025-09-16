@echo off

REM the directory path of this file
set p=%~dp0

REM Unicode version
set UVER=17.0.0

mkdir %p%\data

for %%f in (IdnaMappingTable.txt) do (
  curl -fsS -o %p%\data\%%f https://www.unicode.org/Public/%UVER%/idna/%%f
)

for %%f in (DerivedNormalizationProps.txt UnicodeData.txt) do (
  curl -fsS -o %p%\data\%%f https://www.unicode.org/Public/%UVER%/ucd/%%f
)

for %%f in (DerivedBidiClass.txt DerivedCombiningClass.txt DerivedGeneralCategory.txt DerivedJoiningType.txt) do (
  curl -fsS -o %p%\data\%%f https://www.unicode.org/Public/%UVER%/ucd/extracted/%%f
)
