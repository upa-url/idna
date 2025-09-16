#!/bin/sh

# the directory path of this file
# https://stackoverflow.com/q/59895
p="$(dirname "$0")"

# Unicode version
UVER=17.0.0

mkdir -p $p/data

for f in IdnaMappingTable.txt
do
  curl -fsS -o $p/data/$f https://www.unicode.org/Public/${UVER}/idna/$f
done

for f in DerivedNormalizationProps.txt UnicodeData.txt
do
  curl -fsS -o $p/data/$f https://www.unicode.org/Public/${UVER}/ucd/$f
done

for f in DerivedBidiClass.txt DerivedCombiningClass.txt DerivedGeneralCategory.txt DerivedJoiningType.txt
do
  curl -fsS -o $p/data/$f https://www.unicode.org/Public/${UVER}/ucd/extracted/$f
done
