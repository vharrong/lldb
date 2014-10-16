#!/usr/bin/env bash

CONFIGURE_SCRIPT=llvm/tools/lldb/scripts/Python/lldb_configure.py

# Sync
mkdir -p $HOME/lldb
pushd $HOME/lldb

if [ ! -d "llvm/.git" ]; then
	git clone http://llvm.org/git/llvm.git
	if [ $? != 0 ]; then
		echo "error: clone llvm failed"
		exit 1
	fi
fi

pushd llvm/tools

if [ ! -d "clang/.git" ]; then
	git clone http://llvm.org/git/clang.git
	if [ $? != 0 ]; then
		echo "error: clone clang failed"
		exit 1
	fi
fi

if [ ! -d "lldb/.git" ]; then
	git clone http://llvm.org/git/lldb.git
	if [ $? != 0 ]; then
		echo "error: clone lldb failed"
		exit 1
	fi
fi

popd

# Configure.
# $CONFIGURE_SCRIPT -a -c --cache -g -l -n -s
$CONFIGURE_SCRIPT -a -c -g -l -n -s
if [ $? != 0 ]; then
	echo "error: lldb_configure.py failed"
	exit 1
fi

# Build.
pushd build
time ninja
if [ $? != 0 ]; then
	echo "error: build failed"
	exit 1
fi

# Run tests.
time ninja check-lldb
if [ $? != 0 ]; then
	echo "error: test run failed"
	exit 1
fi
