#!/bin/bash

set -e

REPO="$(dirname $0)/.."
REPO="$(realpath $REPO)"
LCOV_FLAGS="--gcov-tool $(realpath $REPO/tools/llvm-gcov.sh)"

function restore {
	mv config.h.bak $REPO/config/config.h
}

cp $REPO/config/config.h config.h.bak
trap restore EXIT

if [ ! -f "$REPO/config/config.h" ]; then
	echo "$REPO/config/config.h not found"
	exit 1
fi

infos=''

for header in $REPO/config/templates/*.h
do
	filename=$(basename "$header")
	config="${filename%.*}"
	echo
	echo "Build with config: $config"
	echo

	echo '#ifndef PIEX_HEADER_CONFIG_CONFIG' > "$REPO/config/config.h"
	echo '#define PIEX_HEADER_CONFIG_CONFIG' >> "$REPO/config/config.h"
	echo "#include \"config/templates/$filename\"" >> "$REPO/config/config.h"
	echo '#endif' >> "$REPO/config/config.h"

	make tests
	./tests
	info="$config.info"
	infos+="-a $info "
	lcov $LCOV_FLAGS -c -d . -b "$REPO" --no-external -o "$info"
	lcov $LCOV_FLAGS --remove "$info" "$(realpath .)/*" -o "$info"
done

lcov $LCOV_FLAGS $infos -o coverage.info
