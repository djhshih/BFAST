#!/bin/sh

set -eu

cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

sh test.initialize.sh
sh test.fasta2brg.sh
sh test.index.sh
sh test.match.sh
sh test.localalign.sh
sh test.postprocess.sh
sh test.diff.sh
#sh test.cleanup.sh

cd -

