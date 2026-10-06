---
title: otsdaq-mu2e-calorimeter
group: otsdaq-plugins
tier: B
audience: expert
summary: Calorimeter subsystem plugins for otsdaq-mu2e and artdaq-mu2e - the DIRAC/MZB ROC front-end interface, two DTC fragment generators, six art modules for data verification, DQM metrics and analysis, and the table plugin that exports calorimeter channel tables for the online trigger
---

# otsdaq-mu2e-calorimeter

> Calorimeter subsystem plugins for otsdaq-mu2e and artdaq-mu2e: the DIRAC/MZB
> ROC front-end interface, two DTC fragment generators, six art modules for
> data verification, DQM metrics and analysis, and the table plugin that
> exports calorimeter channel tables for the online trigger.

| | |
|---|---|
| Repository | `Mu2e/otsdaq-mu2e-calorimeter` (public, default branch `develop`) |
| Language | C++17 (cetmodules 3.16 build), FHiCL, ROOT macros |
| Version | 9.01.00 (`CMakeLists.txt:13`) |
| Ports | none bound |
| Depends on | `otsdaq-mu2e`, `artdaq-mu2e`, `artdaq-core-mu2e`, `mu2e-pcie-utils`; Offline ≥ 11.00.00 |
| Review tier | B — structural and interface only |

## Purpose

A plugin library. It ships no executables and runs no service; otsdaq and
artdaq processes load its shared objects by plugin name from their
configuration. The interfaces are catalogued in [INTERFACES.md](INTERFACES.md).

## Architecture

```mermaid
flowchart LR
  subgraph otsdaq[otsdaq FE supervisor]
    FE[ROCCalorimeterInterface<br/>25 FE macros]
  end
  subgraph cfg[otsdaq configuration]
    T[(SubsystemCalorimeter*Table)]
    TP[SubsystemCalorimeterParametersTable]
  end
  subgraph br[artdaq BoardReader]
    G[CalorimeterVST /<br/>Mu2eCaloEventReceiver]
  end
  subgraph art[artdaq EventBuilder / DQM art]
    M[CaloDataVerifier<br/>CaloFilterDQM<br/>CaloEventFilter]
    A[CaloDataAnalyzer<br/>CaloiercAnalyzer]
  end
  DTC[DTC over PCIe<br/>mu2e-pcie-utils] -->|ROC registers, MZB SPI| FE
  T --> FE
  T --> TP -->|CalChannels.txt etc.| DB[(DBSERVICE_ONLINE_PATH)]
  DTC -->|DTC events| G -->|Mu2e fragments| M
  M -->|artdaq metrics| MM[metric plugins<br/>graphite in test FHiCL]
  A -->|TFileService| R[(ROOT files)]
  FE -->|ROC Slow Control| L[(slowControl/*.log)]
```

## Components

- `FEInterfaces/` — `ROCCalorimeterInterface`, a `ROCPolarFireCoreInterface`
  subclass from `otsdaq-mu2e`, plus the MZB (mezzanine board) command-encoder
  library `MZB.C`/`MZB.h`.
- `Generators/` — two artdaq commandable generators built on
  `artdaq-mu2e::Mu2eReceiverBase`. `STIBReceiver` is present but not built.
- `ArtModules/` — six art modules; three more sources are present but not built.
- `TablePlugins/` — `SubsystemCalorimeterParametersTable`.
- `boardConfig/` — installed data: per-board CSVs and the slow-control log
  directory with `slow_control_summary.sh`.
- `scripts/` — expert ROOT macros (`otsdaq-mu2e-calorimeter-root-macros(7)`).
- `test/` — hand-run FHiCL; not built.
- `UserWebGUI/` — otsdaq web-app pages; not reviewed.

## Interactions with other packages

| Package | Mechanism | Evidence |
|---|---|---|
| `otsdaq-mu2e` | link: FE base class, `find_package` | `FEInterfaces/CMakeLists.txt:18`, `CMakeLists.txt:71` |
| `artdaq-mu2e` | link: `Mu2eReceiverBase` | `Generators/CMakeLists.txt:5` |
| `artdaq-core-mu2e` | link: overlays and calorimeter decoder | `ArtModules/CMakeLists.txt:7` |
| `mu2e-pcie-utils` | link: `DTCInterface` | `Generators/CMakeLists.txt:5` |
| `otsdaq-mu2e-config` | table schemas (`SubsystemCalorimeter*TableInfo.xml`); sets `DBSERVICE_ONLINE_PATH` | `otsdaq-mu2e-config/hwdev_spack_fast_setup_ots.sh:702` |

## Review findings

Staged in [ISSUES-PENDING.md](ISSUES-PENDING.md) and listed in
[package.yaml](package.yaml).
