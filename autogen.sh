#!/bin/sh
# Regenerate the build system (needs autoconf, automake, pkg-config).
set -e
autoreconf --install --force
