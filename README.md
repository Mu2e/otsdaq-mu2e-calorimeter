# otsdaq-mu2e-calorimeter

Calorimeter subsystem plugins for the Mu2e TDAQ Suite. This package builds no
executables: it is a set of plugins that `otsdaq` and `artdaq` processes load
by name from their configuration.

| Plugin type | Name |
|---|---|
| otsdaq FE interface | `ROCCalorimeterInterface` — DIRAC/MZB ROC on a DTC link; 25 FE macros (configure link, SiPM bias, thresholds, MZB calibration, slow control, board ID from serial) |
| artdaq generators | `CalorimeterVST`, `Mu2eCaloEventReceiver` |
| art modules | `CaloDataVerifier`, `CaloFilterDQM`, `CaloEventFilter` (filters); `CaloDataAnalyzer`, `CaloiercAnalyzer`, `testFragments` (analyzers) |
| otsdaq table plugin | `SubsystemCalorimeterParametersTable` — writes `CalChannels.txt`, `CalChannelStatus.txt`, `CalBaselines.txt` to `$DBSERVICE_ONLINE_PATH` for the online trigger |

## Documentation

- [doc/INTERFACES.md](doc/INTERFACES.md) — every registered plugin, the FE
  macro list with argument names, FHiCL parameters, artdaq metrics, tables,
  environment variables and files
- [doc/ARCHITECTURE.md](doc/ARCHITECTURE.md) — one-page overview and package
  dependencies
- Manual pages: `man/man1/slow_control_summary.sh.1`,
  `man/man7/otsdaq-mu2e-calorimeter-root-macros.7`
  (`man -l man/man1/slow_control_summary.sh.1`)

## Build

Built as part of the Mu2e TDAQ Spack environment (cetmodules). Requires
`otsdaq-mu2e` ≥ 1.02.00 and `Offline` ≥ 11.00.00 (`CMakeLists.txt:71-72`);
Doxygen output is generated only when `OTS_DOXY=DOIT`.

## Runtime environment

| Variable | Used by | Purpose |
|---|---|---|
| `MU2E_CALORIMETER_CONFIG_PATH` | `ROC Slow Control` FE macro | first element + `/slowControl` receives `slowControlNNN.log` / `slowControlVINNN.log` |
| `DBSERVICE_ONLINE_PATH` | `SubsystemCalorimeterParametersTable` | output directory for the trigger channel tables; nothing is written if unset |

## Common DAQ Repositories

* [artdaq-core-mu2e](https://mu2e.github.io/otsdaq-mu2e/doxygen/artdaq-core-mu2e): _artdaq_ Fragment data formats
* [mu2e-pcie-utils](https://mu2e.github.io/otsdaq-mu2e/doxygen/mu2e-pcie-utils): PCIe Device Driver and interface library
* [artdaq-mu2e](https://mu2e.github.io/otsdaq-mu2e/doxygen/artdaq-mu2e): Core data acquisition code
* [otsdaq-mu2e](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e): Slow controls and Front-End Interface
* [otsdaq-mu2e-dqm](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e-dqm): DQM interface and tools
* [mu2e-trig-config](https://mu2e.github.io/otsdaq-mu2e/doxygen/mu2e-trig-config): Trigger path FHiCL generation utilities

## Subsystem-Specific Repositories

* [otsdaq-mu2e-calorimeter](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e-calorimeter)
* [otsdaq-mu2e-crv](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e-crv)
* [otsdaq-mu2e-extmon](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e-extmon)
* [otsdaq-mu2e-stm](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e-stm)
* [otsdaq-mu2e-tracker](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e-tracker)
* [otsdaq-mu2e-trigger](https://mu2e.github.io/otsdaq-mu2e/doxygen/otsdaq-mu2e-trigger)

## License

Fermilab BSD — see [LICENSE](LICENSE).
