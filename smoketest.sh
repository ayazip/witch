#!/usr/bin/env bash

set -e
set -o
set -x

# Print version for reference.
WITCH="./bin/symbiotic" 

$WITCH --version

$WITCH --witness-check smoketest/for-good.yml         --prp=properties/unreach-call.prp smoketest/for.c         | grep "RESULT: false(unreach-call)"
$WITCH --witness-check smoketest/while-bad.yml        --prp=properties/unreach-call.prp smoketest/while.c       | grep "RESULT: true"

$WITCH --witness-check smoketest/termination-good.yml --prp=properties/termination.prp  smoketest/nonterminating-loop.c | grep "RESULT: false(termination)"
$WITCH --witness-check smoketest/termination-bad.yml  --prp=properties/termination.prp  smoketest/nonterminating-loop.c | grep "RESULT: true"


$WITCH --witness-check smoketest/indp1.yml  --prp=properties/valid-memsafety.prp  smoketest/indp1.c | grep "RESULT: false(valid-deref)"

$WITCH --witness-check smoketest/byte_add_1-2.yml  --prp=properties/no-overflow.prp  smoketest/byte_add_1-2.i | grep "RESULT: false(no-overflow)"
