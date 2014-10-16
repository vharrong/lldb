#!/usr/bin/env bash

# Update package list
apt-get update

# Update base system
apt-get upgrade -y

# Install packages needed for building and testing lldb.
apt-get install -y \
	binutils-gold \
	ccache \
	clang-3.5 \
	cmake \
	git \
	libedit-dev \
	ncurses-dev \
	ninja-build \
	python-dev \
	subversion \
	swig \
	tmux
