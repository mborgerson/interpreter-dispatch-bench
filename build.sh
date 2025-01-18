#!/bin/bash
set -e
docker build -t interpreter-dispatch-bench  .
docker run --rm -v $PWD:/work -w /work interpreter-dispatch-bench make
exec ./bench
