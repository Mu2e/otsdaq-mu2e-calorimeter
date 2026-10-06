# Staged issues — otsdaq-mu2e-calorimeter

Eight findings from the September 2026 DAQ mega-review
(`mu2e-sept-mega-review`, 2026-10-06), staged here because GitHub writes are
denied to review agents in this session (CONTRACT.md §9.1). Bodies are in the
CONTRACT.md §6 format, with fingerprints, ready to file unchanged.

`Mu2e/otsdaq-mu2e-calorimeter` is a usable remote: public, issues enabled,
default branch `develop`.

**Dedup** against `gh issue list --state all --limit 300` (2026-10-06): one
issue exists, #24 (OPEN) "Update creation of SubRun transition Fragments",
about the generators' SubRun code. It carries no mega-review fingerprint and
shares no significant title tokens with anything below, so nothing here is a
comment on it. It is referenced from `doc/INTERFACES.md` §4.

**Fingerprints** are `sha1("Mu2e/otsdaq-mu2e-calorimeter:<path>:<rule_id>")[:8]`.

| ID | Sev | Category | rule_id | Fingerprint |
|---|---|---|---|---|
| CAL-001 | S4 | bug | `tracked-build-artifacts` | `d05faf94` |
| CAL-002 | S3 | bug | `test-fcl-broken` | `d0383a18` |
| CAL-003 | S4 | bug | `unbuilt-plugin-sources` | `f0422a54` |
| CAL-004 | S4 | bug | `fe-macro-empty-arg-name` | `b235c71b` |
| CAL-005 | S3 | documentation | `undocumented-env-contract` | `fbf03a3d` |
| CAL-006 | S4 | bug | `unread-data-file` | `bff1a916` |
| CAL-007 | S4 | documentation | `stale-build-instructions` | `64fcdd19` |
| CAL-008 | S4 | bug | `metric-naming` | `6d68023a` |

**Filing:**

```bash
REPO=Mu2e/otsdaq-mu2e-calorimeter
gh label create mega-review -R "$REPO" -c 6F42C1 \
  -d "Filed by the Sept 2026 DAQ mega-review" 2>/dev/null || true
# then one gh issue create per section below, sleeping 4 s between calls
```

---

## CAL-001

**Title:** `[hygiene] compiled ROOT ACLiC artifacts are tracked under scripts/`

**Severity:** S4 — cosmetic, stale, hygiene
**Category:** bug
**Location:** `scripts/converter_C.so`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
Four ACLiC build products are committed next to the ROOT macros they were
built from, including a 410 KB x86-64 ELF shared object. `.gitignore` has no
`*.so`, `*.d` or `*.pcm` rule, so the next ACLiC run in a checkout produces
more of them as untracked files.

## Evidence
```
$ git ls-files scripts | grep -E '\.(so|d|pcm)$'
scripts/converter_C.d
scripts/converter_C.so
scripts/converter_C_ACLiC_dict_rdict.pcm
scripts/dtc_validate_C.d
$ file scripts/converter_C.so
scripts/converter_C.so: ELF 64-bit LSB shared object, x86-64, ..., with debug_info, not stripped
$ head -c 160 scripts/converter_C.d
# DO NOT DELETE
./converter_C.so: /cvmfs/mu2e.opensciencegrid.org/artexternals/root/v6_28_04c/...
$ git log --format='%h %ad %an' --date=short -- scripts/converter_C.so
7a7a86b 2024-03-07 RYAN A RIVERA
```

## Why it matters
The `.so` was built against the UPS-era ROOT v6_28_04c from cvmfs; under the
current Spack ROOT, ACLiC sees a stale library next to the source. It adds a
binary to every clone and to the Doxygen input tree. No operational outage.

## Suggested fix
`git rm --cached scripts/converter_C.so scripts/converter_C.d
scripts/converter_C_ACLiC_dict_rdict.pcm scripts/dtc_validate_C.d` and add
`*_C.so`, `*_C.d`, `*_rdict.pcm` to `.gitignore`.

## Related
- `man/man7/otsdaq-mu2e-calorimeter-root-macros.7` (FILES)

<!-- mega-review-fingerprint: d05faf94 -->

---

## CAL-002

**Title:** `[test] test/ is not built and four of its FHiCL files cannot run against the current plugins`

**Severity:** S3 — correctness or maintainability risk, no current outage
**Category:** bug
**Location:** `test/CaloVerifierFilter_test.fcl:61`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
`test/` is excluded from the build (`CMakeLists.txt:83`,
`#add_subdirectory(test)`), its `CMakeLists.txt` names three sources that do
not exist, and four of its thirteen hand-run FHiCL files fail against the
plugins this package registers:

| File | Defect |
|---|---|
| `test/CaloVerifierFilter_test.fcl:61` | `produceCaloDecoders: true` is not a `CaloDataVerifier` parameter. The module takes a validated `art::EDFilter::Table<Config>` (`CaloDataVerifier_module.cc:34-44`) whose keys are `verbosity`, `dataType`, `metricsLevel`, `stopOnFailure`, `checkEWTs`, `subsystemOverride`, so art rejects the configuration; `caloVerifier` is on trigger path `t1` (`:113`). |
| `test/CalorimeterVST001_test_002.fcl:50`, `_003.fcl:48`, `_004.fcl:39` | `generator: CalorimeterVST001` — no such generator; the registered one is `CalorimeterVST` (`Generators/CMakeLists.txt:8`). |
| `test/iercAnalysis.fcl` | unbalanced braces: the clang-format pass in `f8deece` ("spline update and clang-format fix", 2025-03-19) folded `GeometryService : {` into the `#@ local::Services.Reco` comment on `:12`, leaving one `}` too many at `:27`. It also hard-codes `splineFilename` to `/home/mu2eshift/gr4_process/...` (`:48`). |

Minor, same family: `CalorimeterVST001_test_001.fcl:48` sets `rawOutputFile`,
but the generator reads `raw_output_file` (`CalorimeterVST_generator.cc:187`);
the key is silently ignored (the value happens to equal the default).

## Evidence
```
$ sed -n 1,3p test/CMakeLists.txt
cet_make_exec(ots_udp_sw_emulator SOURCE ots_udp_sw_emulator.cpp)
cet_make_exec(ots_udp_hw_emulator SOURCE ots_udp_hw_emulator.cpp)
cet_make_exec(udp_data_emulator SOURCE udp_data_emulator.cpp)
$ ls test/*.cpp
ls: test/*.cpp: No such file or directory
$ grep -n produceCaloDecoders test/CaloVerifierFilter_test.fcl
61:      produceCaloDecoders : true
$ grep -n 'generator *:' test/CalorimeterVST001_test_00[234].fcl
test/CalorimeterVST001_test_002.fcl:50:    generator              : CalorimeterVST001  ...
test/CalorimeterVST001_test_003.fcl:48:    generator              : CalorimeterVST001  ...
test/CalorimeterVST001_test_004.fcl:39:    generator              : CalorimeterVST001  ...
$ python3 fclbrace.py test/*.fcl        # comment- and string-aware brace counter
test/iercAnalysis.fcl:27: brace depth went negative (-1)
```

## Why it matters
These files are the only executable record of how the generators and art
modules are configured, and the package has no other tests: no `cet_test` or
`add_test` appears anywhere in the build, so the CI "Test Single Pkg"
workflow has no test of this package to run. A reader who copies
them gets a configuration error. The clang-format damage to FHiCL is a
repeatable hazard for any `.fcl` in the repository. S3 rather than S2: these
are test configurations, and no production path loads them.

## Suggested fix
Delete `produceCaloDecoders` from the three files that set it (or add the
parameter if it is wanted), rename `CalorimeterVST001` to `CalorimeterVST`,
restore `iercAnalysis.fcl` from `b892623` with the current spline path made a
relative or `FHICL_FILE_PATH`-resolved name, exclude `*.fcl` from
clang-format, and either delete `test/CMakeLists.txt` or make it build the
FHiCL checks (`fhicl-dump` of each file).

## Related
- CAL-003 (unbuilt sources)

<!-- mega-review-fingerprint: d0383a18 -->

---

## CAL-003

**Title:** `[build] four plugin sources carry registration macros but are never built`

**Severity:** S4 — cosmetic, stale, hygiene
**Category:** bug
**Location:** `otsdaq-mu2e-calorimeter/Generators/CMakeLists.txt:4`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
Four sources register plugins but have no `cet_build_plugin` call, so the
plugins never exist at run time:

| Source | Registration | Notes |
|---|---|---|
| `Generators/STIBReceiver_generator.cc` | `DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(ots::STIBReceiver)` `:100` | header includes `artdaq-ots/Generators/UDPReceiver.hh`; `artdaq-ots` is not a dependency |
| `ArtModules/OtsInput_source.cc` (+ `detail/RawEventQueueReader.{hh,cc}`) | `DEFINE_ART_INPUT_SOURCE(ots::OtsInput)` `:12` | header guards `otsdaq_demo_ArtModules_detail_RawEventQueueReader_hh` |
| `ArtModules/WFViewer_module.cc` | `DEFINE_ART_MODULE(ots::WFViewer)` `:318` | a second copy is tracked as `WFViewer_module.cc.bkup` |

## Evidence
```
$ git grep -n -E 'DEFINE_(ART_MODULE|ART_INPUT_SOURCE|ARTDAQ_COMMANDABLE_GENERATOR)' -- otsdaq-mu2e-calorimeter | wc -l
12          # 11 sources + WFViewer_module.cc.bkup; 8 of the 11 are built
$ grep -h -o 'cet_build_plugin([A-Za-z]*' otsdaq-mu2e-calorimeter/*/CMakeLists.txt
cet_build_plugin(ROCCalorimeterInterface
cet_build_plugin(Mu2eCaloEventReceiver
cet_build_plugin(CalorimeterVST
cet_build_plugin(testFragments
cet_build_plugin(CaloDataVerifier
cet_build_plugin(CaloFilterDQM
cet_build_plugin(CaloDataAnalyzer
cet_build_plugin(CaloiercAnalyzer
cet_build_plugin(CaloEventFilter
cet_build_plugin(SubsystemCalorimeterParametersTable
```

## Why it matters
Doxygen and code search present these as plugins of the package; a FHiCL
author who finds `STIBReceiver` or `WFViewer` gets "plugin not found" at run
time. They are otsdaq-demo leftovers and are not compiled, so they rot
silently. No operational effect.

## Suggested fix
Delete the four sources, `detail/RawEventQueueReader.*` and
`WFViewer_module.cc.bkup`, or add `cet_build_plugin` calls if any is still
wanted.

## Related
- CAL-002

<!-- mega-review-fingerprint: f0422a54 -->

---

## CAL-004

**Title:** `[fe] ReadVoltagesFromDB and ReadChannelStatusFromDB register an input parameter with an empty name`

**Severity:** S4 — cosmetic, stale, hygiene
**Category:** bug
**Location:** `otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface_interface.cc:138`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
Both macros declare their inputs as `std::vector<std::string>{""}` — one input
whose name is the empty string — instead of `{}`.

## Evidence
```
$ sed -n 136,146p otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface_interface.cc
	registerFEMacroFunction("ReadVoltagesFromDB",
	                        ...(&ROCCalorimeterInterface::ReadVoltagesFromDB),
	                        std::vector<std::string>{""},        // inputs parameters
	                        std::vector<std::string>{"Status"},  // output parameters
	                        1);
	registerFEMacroFunction("ReadChannelStatusFromDB",
	                        ...(&ROCCalorimeterInterface::ReadChannelStatusFromDB),
	                        std::vector<std::string>{""},        // inputs parameters
```

## Why it matters
The otsdaq macro interface presents an unlabelled input field to the
operator for these two macros. Cosmetic.

## Suggested fix
Use `std::vector<std::string>{}` for both input lists.

## Related
- `doc/INTERFACES.md` §3.3 (full macro table)

<!-- mega-review-fingerprint: b235c71b -->

---

## CAL-005

**Title:** `[docs] the MU2E_CALORIMETER_CONFIG_PATH and DBSERVICE_ONLINE_PATH runtime contract is documented nowhere`

**Severity:** S3 — correctness or maintainability risk, no current outage
**Category:** documentation
**Location:** `otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface_interface.cc:345`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
Two environment variables change what the plugins do, and neither is
documented in this repository:

- `MU2E_CALORIMETER_CONFIG_PATH` (`_interface.cc:345`): the `ROC Slow
  Control` macro throws if it is unset, and otherwise writes its logs to the
  **first** colon-separated element plus `/slowControl`.
- `DBSERVICE_ONLINE_PATH` (`TablePlugins/SubsystemCalorimeterParametersTable_table.cc:14`):
  if unset or empty, the table plugin silently writes none of the trigger's
  `CalChannels.txt`, `CalChannelStatus.txt`, `CalBaselines.txt`
  (`_table.cc:38-39`).

No repository in `mu2edaq-main` sets `MU2E_CALORIMETER_CONFIG_PATH`. The only
reference is commented out in `otsdaq-mu2e-config`, next to a `source` of a
file that exists in no repository in the workspace.

## Evidence
```
$ grep -rn MU2E_CALORIMETER_CONFIG_PATH ../otsdaq-mu2e-config ../otsdaq-mu2e ../Bootstrap
../otsdaq-mu2e-config/hwdev_spack_fast_setup_ots.sh:463:    #export MU2E_CALORIMETER_CONFIG_PATH="otsdaq-mu2e-calorimeter/boardConfig/"
$ sed -n 461,462p ../otsdaq-mu2e-config/hwdev_spack_fast_setup_ots.sh
    #env vars (THIS_HOST, CONFIG_PATH)
    source ${SCRIPT_DIR}/calo_setup.sh
$ find .. -name calo_setup.sh -not -path '../wt*'
$                                    # no output
$ grep -n DBSERVICE_ONLINE_PATH ../otsdaq-mu2e-config/hwdev_spack_fast_setup_ots.sh
702:export DBSERVICE_ONLINE_PATH=$otsdir/trigger_db_cache/onlinedb_cache
```

## Why it matters
The value that decides where slow-control logs land (and whether the macro
works at all) lives only in an untracked `calo_setup.sh` on the deployment
host. Rebuilding the area from the repositories loses it, and an empty
`DBSERVICE_ONLINE_PATH` gives the trigger stale channel tables with no error.

## Suggested fix
Document both variables in the README (done on the review branch) and have
`otsdaq-mu2e-config` either commit `calo_setup.sh` or export
`MU2E_CALORIMETER_CONFIG_PATH` itself. Make the table plugin log a warning
when `DBSERVICE_ONLINE_PATH` is empty.

## Related
- `doc/INTERFACES.md` §3.2, §6
- Mu2e/otsdaq-mu2e-config (sources the untracked `calo_setup.sh`)

<!-- mega-review-fingerprint: fbf03a3d -->

---

## CAL-006

**Title:** `[fe] boardMap.config is installed and passed to ConfigureLink but no live code reads it`

**Severity:** S4 — cosmetic, stale, hygiene
**Category:** bug
**Location:** `boardConfig/boardMap.config`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
The `Configure Link` macro passes the file name `"boardMap.config"` to the
`ConfigureLink(conf, confFile, ...)` overload, but that overload never uses
`confFile`: every other reference to the name is inside the commented-out
block that begins at `:1499`. Board identity now comes from the
`SubsystemCalorimeterParametersTable` serial → `BoardId` lookup
(`_interface.cc:898-917`). The 161-row file is still tracked and installed.

## Evidence
```
$ grep -n confFile otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface_interface.cc
1293:	std::string confFile = "boardMap.config";
1295:	ConfigureLink(conf, confFile, hvonoff, doCalibration, setThresholds, offset);
1302:void ROCCalorimeterInterface::ConfigureLink(std::string conf, std::string confFile, ...
1512:    std::ifstream confFile(filename);            <- inside /* ... */ from :1499
...
$ git grep -n boardMap -- otsdaq-mu2e-calorimeter
otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterInterface_interface.cc:1293: ...
```
Also tracked and unreferenced: `otsdaq-mu2e-calorimeter/FEInterfaces/board080.config`.

## Why it matters
A board-to-serial map that looks authoritative but is ignored invites edits
that change nothing. If the two sources disagree, the table wins silently.

## Suggested fix
Remove `boardMap.config`, `FEInterfaces/board080.config` and the unused
`confFile` parameter, or state in the file that it is historical.

## Related
- CAL-005

<!-- mega-review-fingerprint: bff1a916 -->

---

## CAL-007

**Title:** `[docs] CMakeLists.txt header documents the retired UPS build`

**Severity:** S4 — cosmetic, stale, hygiene
**Category:** documentation
**Location:** `CMakeLists.txt:1`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
The build file's header tells the reader to `source
.../ups/setup_for_development` and run `buildtool`. There is no `ups/`
directory; the package is built in a Spack environment with cetmodules.

## Evidence
```
$ sed -n 3,6p CMakeLists.txt
#  cd .../path/to/build/directory
#  source .../path/to/otsdaq-mu2e-calorimeter/ups/setup_for_development
#  buildtool
$ ls ups
ls: ups: No such file or directory
```

## Why it matters
The only build instructions in the repository are wrong. Cosmetic.

## Suggested fix
Replace the header with a pointer to the Mu2e TDAQ Spack build instructions.

## Related
- (none)

<!-- mega-review-fingerprint: 64fcdd19 -->

---

## CAL-008

**Title:** `[metrics] CaloDataVerifier metrics have no module prefix and failed_ROCEWTs reuses the DTC description`

**Severity:** S4 — cosmetic, stale, hygiene
**Category:** bug
**Location:** `otsdaq-mu2e-calorimeter/ArtModules/CaloDataVerifier_module.cc:184`
**Found by:** mega-review (`mu2e-sept-mega-review`), 2026-10-06

## What
`CaloFilterDQM` prefixes every metric with `CaloFilterDQM.`; `CaloDataVerifier`
sends bare names (`nHits`, `nCaloDTCs`, `failed_*`). The description of
`failed_ROCEWTs` is a copy of the DTC one.

## Evidence
```
$ grep -n 'sendMetric("' otsdaq-mu2e-calorimeter/ArtModules/CaloDataVerifier_module.cc | cut -c1-110
183:		metricMan->sendMetric("failed_DTCEWTs", event_failed_DTCEWTs, "Errors in DTC EWT", ...
184:		metricMan->sendMetric("failed_ROCEWTs", event_failed_ROCEWTs, "Errors in DTC EWT", ...
...
191:		metricMan->sendMetric("nCaloDTCs", int(nCaloEvents), "Calo DTCs", ...
192:		metricMan->sendMetric("nHits", int(nCaloHits), "Hits per event", ...
```

## Why it matters
In a Graphite/Grafana namespace shared with other subsystems' verifiers, a
bare `nHits` does not say which detector it counts. Cosmetic today.

## Suggested fix
Prefix with `CaloDataVerifier.` (as `CaloFilterDQM` does) and fix the
`failed_ROCEWTs` description to "Errors in ROC EWT". Update any Grafana panels
that query the old names in the same change.

## Related
- `doc/INTERFACES.md` §5.1 (metric table)

<!-- mega-review-fingerprint: 6d68023a -->
