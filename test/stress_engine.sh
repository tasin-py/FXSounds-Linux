#!/bin/sh
# Stress test: thousands of random preset/slider/EQ/power changes while audio plays through FxSound.
# Run inside a PipeWire session with the sanitizer build:  BIN=build-san/fxsound-engine sh test/stress_engine.sh
cd "$(dirname "$0")/.."
BIN="${BIN:-build/fxsound-engine}"
N="${N:-3000}"
T="${T:-/tmp/fxstress}"
mkdir -p "$T"
awk -v n="$N" 'BEGIN{srand(1); for(i=0;i<n;i++){r=int(rand()*5);
  if(r==0) printf "preset upstream/presets/%d.fac\n", 1+int(rand()*12);
  else if(r==1) printf "effect %d %.1f\n", int(rand()*5), rand()*10;
  else if(r==2) printf "eq %d %.1f\n", int(rand()*10), rand()*24-12;
  else if(r==3) printf "power %s\n", (rand()<0.5?"on":"off");
  else print "status"}
  print "power on"; print "status"}' > "$T/cmds.txt"
(sleep 1.5; while read -r l; do echo "$l"; sleep 0.003; done < "$T/cmds.txt"; sleep 1; echo quit) | "./$BIN" > "$T/stress.log" 2>&1 &
E=$!
sleep 1
timeout 25 pw-play --target fxsound test/test_input.wav
wait $E
echo "engine exit $?"
grep -E "ERROR|runtime error|Sanitizer|failed|leak" "$T/stress.log" | head -20
grep -E "^power on, rate" "$T/stress.log" | tail -1
grep "^capture" "$T/stress.log" | tail -1
