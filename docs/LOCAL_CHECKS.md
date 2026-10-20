# Checks performed for this delivery

- g++ -std=c++17 -Wall -Wextra -pedantic: pulse tests compiled and passed.
- Baseline/zero signal, known-charge isolated pulse, clipped/saturated edge pulse,
  and invalid configuration checked by the existing pure pulse tests.
- MDPulse.h and MOLLERMDReader.h are byte-identical to the working prototype.
- Included digitiser files are byte-identical to the prior all-224 snapshot.
- Replay DB: 224 unique detector IDs and 224 unique ROC/slot/channel addresses,
  matching the existing enabled-channel CSV exactly.
- Every moller.md output definition matches a detector RVarDef.
- Bash setup/launcher syntax checked; launcher tested with an analyzer stand-in
  for argument quoting and rejection of identical input/output paths.

Not tested here: CMake configuration/build (CMake unavailable), ROOT dictionary
build, Podd shared-library loading, native database parsing, slot-data population,
and end-to-end detector replay. These require the provided ifarm build and
100-event validation instructions. No new replay performance numbers are claimed.
