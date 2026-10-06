---
title: otsdaq-mu2e-calorimeter interfaces
group: otsdaq-plugins
tier: B
audience: expert
summary: Calorimeter subsystem plugins for otsdaq-mu2e and artdaq-mu2e - the DIRAC/MZB ROC front-end interface, two DTC fragment generators, six art modules for data verification, DQM metrics and analysis, and the table plugin that exports calorimeter channel tables for the online trigger
---

# otsdaq-mu2e-calorimeter — interfaces

> Calorimeter subsystem plugins for otsdaq-mu2e and artdaq-mu2e: the DIRAC/MZB
> ROC front-end interface, two DTC fragment generators, six art modules for data
> verification, DQM metrics and analysis, and the table plugin that exports
> calorimeter channel tables for the online trigger.

This is a Tier B (structural) review. Everything below was taken from
`CMakeLists.txt` files, public headers, the plugin registration macros
(`DEFINE_OTS_*`, `DEFINE_ARTDAQ_*`, `DEFINE_ART_MODULE`), the
`registerFEMacroFunction` calls, the `*.fcl` files and the shell script. FHiCL
parameter *names* for modules without a header were taken from a grep of their
`fhicl::Name(...)` / `ps.get<>("...")` declarations; their semantics and
defaults were not reviewed unless stated. Implementation bodies were not read.

Version: `project(otsdaq-mu2e-calorimeter VERSION 9.01.00)` (`CMakeLists.txt:13`).

## 1. What builds, and what does not

The top-level build adds exactly one source subtree and one data directory:

| `CMakeLists.txt` line | Effect |
|---|---|
| `:1-7` | header comment still gives the UPS-era recipe (`source .../ups/setup_for_development`, `buildtool`); there is no `ups/` directory (finding CAL-007) |
| `:71` | `find_package(otsdaq-mu2e 1.02.00 REQUIRED)` |
| `:72` | `find_package(Offline 11.00.00 REQUIRED)` |
| `:77` | `add_subdirectory(otsdaq-mu2e-calorimeter)` → `FEInterfaces`, `Generators`, `ArtModules`, `TablePlugins` |
| `:80` | `#add_subdirectory(tools)` — commented out; `tools/` holds only a placeholder `CMakeLists.txt` |
| `:83` | `#add_subdirectory(test)` — commented out; see §8 |
| `:86-88` | `doc/` (Doxygen) only when `OTS_DOXY=DOIT` |
| `:90-91` | `boardConfig/` copied into the build tree and installed to the install prefix |

Sources present in the tree but **not built** by any `cet_build_plugin` call
(finding CAL-003):

| File | Registration macro present | Why it does not load |
|---|---|---|
| `Generators/STIBReceiver_generator.cc` | `DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(ots::STIBReceiver)` (`:100`) | not in `Generators/CMakeLists.txt`; its header includes `artdaq-ots/Generators/UDPReceiver.hh`, a package this build never finds |
| `ArtModules/OtsInput_source.cc` + `ArtModules/detail/RawEventQueueReader.{hh,cc}` | `DEFINE_ART_INPUT_SOURCE(ots::OtsInput)` (`:12`) | not in `ArtModules/CMakeLists.txt`; header guards still read `otsdaq_demo_...` |
| `ArtModules/WFViewer_module.cc` (+ tracked `WFViewer_module.cc.bkup`) | `DEFINE_ART_MODULE(ots::WFViewer)` (`:318`) | not in `ArtModules/CMakeLists.txt` |

## 2. Registered plugins

| Type | Plugin name (as used in configuration) | Build line | Registration |
|---|---|---|---|
| otsdaq FE interface | `ROCCalorimeterInterface` | `FEInterfaces/CMakeLists.txt:15` | `ROCCalorimeterInterface_interface.cc:2810` `DEFINE_OTS_INTERFACE` |
| support library | `otsdaq-mu2e-calorimeter::FEInterfaces` (MZB command encoder, `MZB.C`/`MZB.h`) | `FEInterfaces/CMakeLists.txt:4` | — |
| artdaq generator | `Mu2eCaloEventReceiver` | `Generators/CMakeLists.txt:4` | `Mu2eCaloEventReceiver_generator.cc:151` `DEFINE_ARTDAQ_COMMANDABLE_GENERATOR` |
| artdaq generator | `CalorimeterVST` | `Generators/CMakeLists.txt:8` | `CalorimeterVST_generator.cc:914` |
| art module (EDAnalyzer) | `testFragments` | `ArtModules/CMakeLists.txt:6` | `testFragments_module.cc:74` |
| art module (EDFilter) | `CaloDataVerifier` | `ArtModules/CMakeLists.txt:10` | `CaloDataVerifier_module.cc:749` |
| art module (EDFilter) | `CaloFilterDQM` | `ArtModules/CMakeLists.txt:17` | `CaloFilterDQM_module.cc:383` |
| art module (EDAnalyzer) | `CaloDataAnalyzer` | `ArtModules/CMakeLists.txt:27` | `CaloDataAnalyzer_module.cc:570` |
| art module (EDAnalyzer) | `CaloiercAnalyzer` | `ArtModules/CMakeLists.txt:40` | `CaloiercAnalyzer_module.cc:495` |
| art module (EDFilter) | `CaloEventFilter` | `ArtModules/CMakeLists.txt:53` | `CaloEventFilter_module.cc:146` |
| otsdaq table plugin | `SubsystemCalorimeterParametersTable` | `TablePlugins/CMakeLists.txt:4` | `SubsystemCalorimeterParametersTable_table.cc:266` `DEFINE_OTS_TABLE` |

There are no data-processor plugins and no executables. Nothing in this package
binds a network port.

## 3. FE interface — `ROCCalorimeterInterface`

`class ROCCalorimeterInterface : public ROCPolarFireCoreInterface`
(`FEInterfaces/ROCCalorimeterInterface.h:13`). The base classes live in
`otsdaq-mu2e`: `otsdaq-mu2e/FEInterfaces/ROCPolarFireCoreInterface.h` →
`otsdaq-mu2e/ROCCore/ROCCoreVInterface.h` → otsdaq `FEVInterface`. One
instance is created per calorimeter ROC (DIRAC board) on a DTC link; register
access goes through the parent DTC (`readROCBlock`, `universalBlockRead`
overrides, `ROCCalorimeterInterface.h:34-35`).

State-machine overrides: `configure()`, `start(runNumber)`, `running()`
(`.h:22-28`); `halt/pause/resume/stop` are inherited. Emulator hooks:
`writeEmulatorRegister`, `readEmulatorRegister`, `emulatorWorkLoop`.

The ROC register map the interface writes is the `CaloRegisters` enum
(`.h:39-83`): e.g. `ROC_ADDRESS_WORKMODE=122`, event-window length/blind/delay
`123/124/125`, channel masks `120/121`, base threshold `100`, MZB busy `140`,
board ID `142`, 48-bit board unique ID in `145/146/147`, simulated-waveform
controls `150-153`. Block reads of addresses `{256,257,260-267}` are treated
specially (word count written first) — `ROCCalorimeterInterface_interface.cc:30`.

### 3.1 Configuration-tree inputs

| Source | Field | Use |
|---|---|---|
| ROC record (`getSelfNode()`) | `inputTemperature` (double, default 15.0) | emulator thermometer seed (`_interface.cc:42-46`) |
| `SubsystemCalorimeterParametersTable` | `SerialNumber`, `BoardId` | `updateBoardIdFromSerial_()`: reads ROC register 147, finds the row whose `SerialNumber` matches, writes its `BoardId` to register 142 (`_interface.cc:898-917`) |
| `SubsystemCalorimeterThresholdsTable` | per-board record | `_interface.cc:855` |
| `SubsystemCalorimeterMZBCalibsTable` | per-board record | `_interface.cc:1362` |
| `SubsystemCalorimeterVoltagesTable` | per-board record | `_interface.cc:1587` |
| `SubsystemCalorimeterStatusTable` | per-board record | `_interface.cc:1738` |

The four per-board records are cached in `CaloBoardConfig` (20 channels per
board, `.h:169-196`; `MAX_BOARD_ID = 160`, `.h:166`). Table names are from a
grep of the `getNode("...Table")` calls; the column names other than the two
above were not reviewed. The table schemas are not in this package:
they are `otsdaq-mu2e-config/Data_*/TableInfo/SubsystemCalorimeter*TableInfo.xml`
(`ParametersTable` appears in 11 `Data_*` directories, `MZBCalibs` and
`Voltages` in one each).

### 3.2 Environment and files

| Name | Read at | Meaning |
|---|---|---|
| `MU2E_CALORIMETER_CONFIG_PATH` | `_interface.cc:345` | colon-separated path; `ROC Slow Control` writes its logs to the **first** element's `slowControl/` subdirectory, creating it if needed, and throws if the variable is unset. This is the only live read: the `cet::filepath_lookup("MU2E_CALORIMETER_CONFIG_PATH")` at `:1509` sits inside a commented-out `SetADCsThresholds` overload that begins at `:1499`. |

No repository in `mu2edaq-main` sets `MU2E_CALORIMETER_CONFIG_PATH`: the only
reference is a commented-out export in
`otsdaq-mu2e-config/hwdev_spack_fast_setup_ots.sh:463`, beside a `source
${SCRIPT_DIR}/calo_setup.sh` (`:462`) for a file that exists in no repository in
the workspace (finding CAL-005).

Files under `boardConfig/` (installed by `CMakeLists.txt:91`):

| Path | Content |
|---|---|
| `boardMap.config` | 161 rows of three whitespace-separated columns: 3-digit board ID, a 16-bit value (36097 or 36353), and a board serial. The `Configure Link` macro passes the name `"boardMap.config"` (`_interface.cc:1293`) to the `ConfigureLink(conf, confFile, ...)` overload (`:1302`), but no live line references `confFile` after that — the remaining references (`:1512-1548`) are inside the commented-out block that starts at `:1499`. Board identity now comes from the serial → `BoardId` table lookup (§3.1), so no live code reads this file (finding CAL-006). |
| `configurations/voltages/diracEqualized*.csv` | one row per board: 20-channel SiPM bias voltages and sensor types (`CAL`, `CAPHRI`), with source-file, author and date columns — the same record shape as the configuration tables the FE interface loads in §3.1 |
| `configurations/thresholds/thresholds_run120788.csv` | per-board 20-channel baselines and thresholds |
| `configurations/channelstatus/channelStatus.csv` | per-board 20-channel status strings |
| `configurations/mezzanine/mzbCalib.csv` | per-board MZB ADC calibration constants |
| `slowControl/slow_control_summary.sh` | collects the last line of each per-board slow-control log — see `slow_control_summary.sh(1)` |

`FEInterfaces/board080.config` (20 rows of channel, voltage, sensor type) is
tracked but not referenced by any code or build file.

### 3.3 Registered FE macros

Registered in the constructor, `ROCCalorimeterInterface_interface.cc:59-255`.
Every macro is registered with `requiredUserPermissions = 1`. Input and output
names are verbatim, including the stray `]` characters in the source.

| Macro name | Inputs | Outputs | Handler |
|---|---|---|---|
| `Setup for Fixed-length Pattern Data Taking` | `Fixed Length of Event [units of 16-bit words, Default := 8]` | — | `SetupForPatternFixedLengthDataTaking` |
| `Send Mz Command` | `command tag from mz manual`, `argument 0, Default := 0]` … `argument 9, Default := 0]` | — | `SendMzCommand` |
| `Enable and Power SiPMs` | `HV Enabled, Default := 0]`, `Bias voltage to set, Default := 0]` | — | `EnableAndPowerSiPMs` |
| `Enable/Disable MZB Leds` | `Enable LEDs, Default := 0]` | — | `EnableDisableLEDs` |
| `Find BoardID From Serial` | — | `Status` | `FindBoardIDFromSerial` |
| `Set Board Voltages` | `configuration folder, Default:= nominal`, `Left/Right, Default := 0]`, `Board Number in Crate, Default := 0]`, `Crate Number, Default := 0]`, `Half Number, Default := 0]`, `Disk Number, Default := 0]`, `Board ID, Default := -1]`, `HV Enabled, Default := 0]` | — | `SetBoardVoltages` |
| `Configure Link` | `Configuration folder, Default:= nominal`, `HV Enabled, Default := 0]`, `Upload MZB calibration parameters, Default := 0]`, `Upload DiRAC thresholds, Default := 0]`, `Offset, Default := 0]` | — | `ConfigureLink` |
| `Calibrate Mzb` | — | — | `CalibrateMZB` |
| `SetADCsThresholds` | `Offset, Default := 0]` | — | `SetADCsThresholds` |
| `ReadVoltagesFromDB` | one input with an **empty name** (`""`, finding CAL-004) | `Status` | `ReadVoltagesFromDB` |
| `ReadChannelStatusFromDB` | one input with an **empty name** (`""`) | `Status` | `ReadChannelStatusFromDB` |
| `Print ROC Configuration` | — | `Status` | `PrintROCConfiguration` |
| `SetupForNoiseTaking` | `Number of noise samples per evt [Default := 20]` | — | `SetupForNoiseTaking` |
| `Read ROC Error Counter` | `Address to read, Default := 0]` | `Status` | `ReadROCErrorCounter` |
| `Setup for ADCs Data Taking` | `Set Threshold? [bool, Default := 0]`, `Threshold [units of adccounts, Default := 2300]`, `Is new Firmware? [bool, Default := 0]` | — | `SetupForADCsDataTaking` |
| `Toggle MB Busy for Noise` | `On/Off [Busy On/Off, Default := 1]` | — | `ToggleMBBusy` |
| `ROC Status` | — | `Status` | `GetStatus` (override) |
| `MZB Status` | — | `Status` | `GetMZBStatus` |
| `ROC Slow Control` | — | `Status` | `ROCSlowControl` — appends a timestamped line per board to `slowControlNNN.log` and `slowControlVINNN.log` |
| `TRAD Slow Control` | — | `Status` | `TRADSlowControl` |
| `TRAD Set Mask` | `Set Mask as series of bit (0 enabled, 1 disabled) Default := 0` | — | `TRADSetMask` |
| `Read MB Registers` | `Number of 16 bits words to read, Default := 20]`, `Mezzanine Address, Default := 0]` | `Status` | `ReadMBRegisters` |
| `Evaluate BlockWrite Error Rate` | `Number of test loops, Default := 900]`, `Number of 16-bits words in a block transfer, Default := 500]` | `Status` | `EvaluateBlockWriteErrorRate` |
| `Configure State Machine` | — | — | `Configure` |
| `Create Global ROC Table` | — | `Status` | `CreateGlobalROCTable` |

That is 25 macros. Commented-out registrations (`Setup for Pattern Data
Taking`, `SetROCCaloVoltageChannel`, `GetROCCaloVoltageChannel`,
`GetROCCaloTemperatureChannel`) are not registered.

Inherited from `otsdaq-mu2e` and therefore also available on every
calorimeter ROC (names from a grep of the base-class registration calls):

| Registered in | Macro names |
|---|---|
| `otsdaq-mu2e/ROCCore/ROCCoreVInterface.cc:62-68` | `Get ROC Status`, `Get ROC Firmware Version` |
| `otsdaq-mu2e/FEInterfaces/ROCPolarFireCoreInterfaceImpl.cc:20-51` | `Setup for Pattern Data Taking`, `Read SPI Flash Block`, `Write SPI Flash Directory`, `Erase SPI Flash Block`, `Force Clear Action Lock` |

`ROC Status` (this package) and `Get ROC Status` (base) both dispatch to the
virtual `GetStatus`, so they run the same calorimeter override.

## 4. artdaq generators

Both generators link `artdaq-mu2e::Mu2eReceiverBase`, `artdaq-core-mu2e::Overlays`
and `mu2e-pcie-utils::DTCInterface` (`Generators/CMakeLists.txt:4-9`): they
read DTC events over PCIe and emit `Mu2e`-typed fragments. Parameters shared
with the base class are documented in `artdaq-mu2e`.

| Generator | Parameter names read by this package (grep) | Exercised by |
|---|---|---|
| `CalorimeterVST` | `activeLinks`, `asyncRR`, `board_id`, `cfo_config` (`use_dtc_cfo_emulator`, `debug_packet_count`, `debug_type`, `sticky_debug_type`, `quiet`, `force_no_debug_mode`, `useCFODRP`), `debugLevel`, `delay_between_requests_ticks`, `dtcId`, `fragment_ids`, `heartbeatInterval`, `heartbeatsAfter`, `loadSimFile`, `maxEventsPerSubrun`, `nEventsDbg`, `printFreq`, `raw_output_file`, `rawOutputEnable`, `readData`, `readoutMode`, `resetROC`, `saveDTCRegisters`, `saveSPI`, `sendEmpties`, `sim_mode`, `simFileName`, `sleepTimeDMA`, `sleepTimeDTC`, `sleepTimeROC`, `sleepTimeROCReset` | `test/driver_calorimeter_vst.fcl:46`, `test/CalorimeterVST001_test_001.fcl:40-85` (`artdaqDriver -c ...`) |
| `Mu2eCaloEventReceiver` | `fragment_ids`, `sim_mode` | `test/test_001.fcl:17` |

Note that `CalorimeterVST001_test_001.fcl:47-48` sets `rawOutputEnable` and
`rawOutputFile`, but the generator reads `raw_output_file`; the camel-case key
is ignored (non-validated `ParameterSet`). Open upstream issue
[#24](https://github.com/Mu2e/otsdaq-mu2e-calorimeter/issues/24) tracks the
outdated SubRun-fragment code in both generators.

## 5. art modules

All six decode `artdaq::Fragments` of type `mu2e::FragmentType::DTCEVT`
(directly or inside a `ContainerFragment`) through
`mu2e::DTCEventFragment` and `mu2e::CalorimeterDataDecoder` from
`artdaq-core-mu2e/Overlays`, except `CaloiercAnalyzer`, which consumes
`mu2e::CaloDigiCollection` from Offline. None declares `produces<>`.

| Module | Kind | FHiCL parameters | Reads | Emits |
|---|---|---|---|---|
| `CaloDataVerifier` | EDFilter, validated `Table<Config>` | `verbosity` (int, 0), `dataType` (int, 0: standard, 1: debug, 2: counters), `metricsLevel` (int, 1), `stopOnFailure` (bool, false), `checkEWTs` (bool, false), `subsystemOverride` (string, `"calo"` or `"tracker"`) — `CaloDataVerifier_module.cc:34-41` | DTCEVT fragments | artdaq metrics (§5.1); filter decision |
| `CaloFilterDQM` | EDFilter | `verbosity`, `metricsLevel` | DTCEVT fragments; CFO data | artdaq metrics `CaloFilterDQM.*` (§5.1) |
| `CaloDataAnalyzer` | EDAnalyzer | `dataType`, `fillEmptyEvents`, `fillFailedEvents`, `maxEventNum`, `verbosity` | DTCEVT fragments | `TFileService`: `h1_t0`, `h1_maxIndex`, `h1_nSamples`, `h2_channelHits`, `h2_failedChannels`, `h2_failedROCs`, `h2_waveforms`; TTrees `tree`, `treeBad`, `treeHits` |
| `CaloiercAnalyzer` | EDAnalyzer | `caloDigiModuleLabel`, `caloDigiInstanceLabel`, `verbosity`, `splineFilename`, `skipAfterN`, `produceTree`, `uset0` | `mu2e::CaloDigiCollection`; spline ROOT file | `TFileService` tree `pulseIERCAnalysis/tree` (read back by `scripts/calo_gr4_generate_txt.C`) |
| `CaloEventFilter` | EDFilter | `debugEvery`, `subsystemOverride` | DTCEVT fragments | filter decision (drops events with no calorimeter payload; used as `filterEmptyEvents` in `test/makeRootTreeFromArt.fcl:18-22`) |
| `testFragments` | EDAnalyzer | none | fragments | `TFileService` histograms |

### 5.1 Metrics export — where Grafana gets calorimeter numbers

The package contains **no** Grafana, Prometheus or EPICS code. Its only
monitoring export is the artdaq `MetricManager` (`metricMan->sendMetric`),
which forwards to whatever metric plugins the enclosing artdaq process
configures under `daq.metrics`. The only plugin configuration in this
repository is in two test FHiCL files, which point a `graphite` plugin at
`mu2e-dcs-01.fnal.gov:2003` with namespace `test.pgirotti.`
(`test/CaloFilterDQM_test.fcl:4-12`, `test/CaloVerifierFilter_test.fcl:8-16`).
The production routing is not in this repository.

| Module | Metric name | Mode | Line |
|---|---|---|---|
| `CaloDataVerifier` | `failed_DTCEWTs`, `failed_ROCEWTs`, `failed_ROCCounters`, `failed_Counters`, `failed_BeginMarker`, `failed_LastMarker` | Accumulate | `CaloDataVerifier_module.cc:183-188` |
| | `nCaloDTCs`, `nHits` | LastPoint | `:191-192` |
| `CaloFilterDQM` | `CaloFilterDQM.nEventBadDTCEWTs`, `.nEventBadROCEWTs`, `.nEventBadHitEWTs` | Maximum | `CaloFilterDQM_module.cc:143-145` |
| | `.nBadHitEWTChannels`, `.nTotalHits`, `.nTotalBadHits`, `.nTotalGoodHits`, `.FailedHits_<errorName>`, `.FailedDTCLinks`, `.FailedHits_DTC<d>_Link<r>`, `.GoodHits_DTC<d>_Link<r>`, `.nCaloDTCs` | LastPoint | `:146-183` |
| | `.nHits`, `.nGoodHits`, `.nBadHits` | Average | `:184-186` |

The `CaloDataVerifier` metric names carry no module prefix, and the description
string of `failed_ROCEWTs` (`:184`) is a copy of the DTC one ("Errors in DTC
EWT") (finding CAL-008).

## 6. Table plugin — `SubsystemCalorimeterParametersTable`

`class SubsystemCalorimeterParametersTable : public TableBase`
(`TablePlugins/SubsystemCalorimeterParametersTable.h:9`). On `init()` — and
only in the first application of the context (`isOwnerFirstAppInContext`) — it
reads three otsdaq tables and writes three CSV files for the online trigger:

| Input table (`_table.cc:15-17`) | Columns used (`.h:43-60`) |
|---|---|
| `SubsystemCalorimeterMapTable` | `offID`, `FEEchan` |
| `SubsystemCalorimeterStatusTable` | `BoardID`, `Status` |
| `SubsystemCalorimeterThresholdsTable` | `BoardID`, `Baseline`, `Thresholds` |

| Output (`_table.cc:68-72`) | Path |
|---|---|
| `CalChannels` | `$DBSERVICE_ONLINE_PATH/CalChannels.txt` |
| `CalChannelStatus` | `$DBSERVICE_ONLINE_PATH/CalChannelStatus.txt` |
| `CalBaselines` | `$DBSERVICE_ONLINE_PATH/CalBaselines.txt` |

If `DBSERVICE_ONLINE_PATH` is unset or empty, `init()` returns without writing
(`_table.cc:38-39`). `otsdaq-mu2e-config/hwdev_spack_fast_setup_ots.sh:702`
sets it to `$otsdir/trigger_db_cache/onlinedb_cache`. The CRV table plugin
(`otsdaq-mu2e-crv`) writes to the same directory. `getStructureAsJSON()` is
also overridden for the otsdaq table GUI.

## 7. Scripts and tools

- `scripts/` — six ROOT/ACLiC macros, indexed in
  `otsdaq-mu2e-calorimeter-root-macros(7)`. Four compiled ACLiC artifacts are
  tracked next to them (finding CAL-001).
- `boardConfig/slowControl/slow_control_summary.sh` — `slow_control_summary.sh(1)`.
- `tools/` — empty placeholder, not built.

## 8. Tests and CI

`test/` is not part of the build (`CMakeLists.txt:83`), and
`test/CMakeLists.txt:1-3` names three sources (`ots_udp_sw_emulator.cpp`,
`ots_udp_hw_emulator.cpp`, `udp_data_emulator.cpp`) that do not exist. The
13 FHiCL files under `test/` are run by hand with `art -c` or `artdaqDriver
-c`; four of them cannot run against the current plugins (finding CAL-002):

| File | Defect |
|---|---|
| `CaloVerifierFilter_test.fcl:61` | `produceCaloDecoders` is not a `CaloDataVerifier` parameter; the module's validated `Table<Config>` rejects it, and `caloVerifier` is on trigger path `t1` |
| `CalorimeterVST001_test_002.fcl:50`, `_003.fcl:48`, `_004.fcl:39` | `generator: CalorimeterVST001` — no such plugin; the registered name is `CalorimeterVST` |
| `iercAnalysis.fcl` | unbalanced braces (an extra `}` at `:27`) since commit `f8deece` ran clang-format over it; also hard-codes `/home/mu2eshift/gr4_process/...` for `splineFilename` (`:48`) |

CI: eight workflows under `.github/workflows/`, all calling reusable
workflows in `Mu2e/.github@stable` (build, develop build, format, tidy, test,
whitespace) plus two project-board trackers.

## 9. UserWebGUI

`UserWebGUI/` holds otsdaq web-app assets (`CaloConfiguration.html`,
`Calorimeter_Test.html`, `FEStructure_calo.html`, `NimPlus_app.html`,
`SmartLaunch.html`, `ConfigurationAPI_exampleApp.html`) plus a vendored copy of
MaterialDesign-Webfont. They are served by the otsdaq gateway, not by this
package, and are not documented here.
