#!/bin/bash
mkdir _tmp_stylecheck
cd _tmp_stylecheck
git clone https://github.com/rsyslog/codestyle
cd codestyle
gcc --std=c99 stylecheck.c -o stylecheck
cd ../..
find . -name "*.[ch]" -exec _tmp_stylecheck/codestyle/stylecheck '{}' +
rm -rf _tmp_stylecheck
