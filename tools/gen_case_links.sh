#!/bin/sh
# The upstream code was written on Windows, where file names are case-insensitive,
# so it includes e.g. "Pt_defs.h" while the file is "pt_defs.h", or <Windows.h> for windows.h.
# For every #include that has no exact-case match on disk, create a symlink with the
# requested spelling in the output directory (which is on the include path).
set -eu
out="$1"
rm -rf "$out"; mkdir -p "$out"
root="$(pwd)/upstream"
compat="$(pwd)/compat"

# all headers we have, as "lowercase-basename<TAB>path"
find "$root" "$compat" -type f \( -iname '*.h' -o -iname '*.hpp' \) | while read -r f; do
  b=$(basename "$f"); printf '%s\t%s\n' "$(printf '%s' "$b" | tr 'A-Z' 'a-z')" "$f"
done > "$out/.index"

# every name used in #include "..." or #include <...>
grep -rhoE '^[[:space:]]*#[[:space:]]*include[[:space:]]*["<][^">]+[">]' "$root" \
  --include='*.c' --include='*.cpp' --include='*.h' --include='*.H' \
  | sed -E 's/.*["<]([^">]+)[">].*/\1/' | sort -u | while read -r name; do
  base=${name##*/}; base=${base##*\\}
  # exact spelling exists somewhere? then nothing to do
  if grep -q "/$base\$" "$out/.index"; then continue; fi
  lc=$(printf '%s' "$base" | tr 'A-Z' 'a-z')
  target=$(awk -F'\t' -v k="$lc" '$1==k {print $2; exit}' "$out/.index")
  if [ -n "$target" ]; then ln -sf "$target" "$out/$base"; fi
done
rm -f "$out/.index"
echo "case links: $(ls "$out" | wc -l) created in $out"
