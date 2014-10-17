#!/bin/sh

# Update package list
pkg update

# Update base system
pkg upgrade -y

# Install packags needed to build.
pkg install -y \
	bash \
	clang35 \
	cmake \
	git \
	ninja \
	python \
	swig13 \
	tmux
