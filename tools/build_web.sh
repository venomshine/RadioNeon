#!/bin/bash
# web/history.html -> romfs/web/history.html.gz (the console sends it gzipped as it is).
# Checks the JavaScript first. Run it after editing the page.
set -e
cd "$(dirname "$0")/.."
python3 - <<'PY'
import re
s = open('web/history.html').read()
open('/tmp/rn_page.js', 'w').write(re.search(r'<script>(.*)</script>', s, re.S).group(1))
PY
node --check /tmp/rn_page.js
mkdir -p romfs/web
gzip -9n -c web/history.html > romfs/web/history.html.gz
ls -l romfs/web/history.html.gz
