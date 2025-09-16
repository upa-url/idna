#!/bin/sh

# the directory path of this file
# https://stackoverflow.com/q/59895
p="$(dirname "$0")"

# Unicode version
UVER=17.0.0

for f in IdnaTestV2.txt
do
  curl -fsS -o $p/data/$f https://www.unicode.org/Public/${UVER}/idna/$f
done

for f in NormalizationTest.txt
do
  curl -fsS -o $p/data/$f https://www.unicode.org/Public/${UVER}/ucd/$f
done
