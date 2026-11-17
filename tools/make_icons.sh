#!/bin/sh
# Converts FxSound's Windows .ico status icons to PNG (largest frame) for the Linux tray/window.
cd "$(dirname "$0")/../linux/icons"
for n in red_logo blue_logo white_logo gray_logo icon; do
  f=ico/$n.ico
  best=$(magick identify "$f" 2>/dev/null | awk '{print NR-1, $3}' | sort -t x -k1,1n -k2 -n | awk '{split($2,a,"x"); print a[1]*a[2], $1}' | sort -n | tail -1 | cut -d' ' -f2)
  magick "$f[$best]" "$n.png"
  echo "$n: $(magick identify -format '%wx%h' $n.png)"
done
for s in 16 22 24 32 48 64 128 256; do magick icon.png -resize ${s}x${s} fxsound-$s.png; done
