#!/bin/bash
if [ "$1" == "-v" ]; then
	exec llvm-cov gcov --version
else
	exec llvm-cov gcov "$@"
fi
