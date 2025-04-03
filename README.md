<p align="center">
  <img src="https://img.shields.io/badge/ESEE-Foundation-orange" alt="ESEE Foundation">
  <a href="https://github.com/ecmwf/codex/blob/refs/heads/main/Project%20Maturity/readme.md">
    <img src="https://img.shields.io/badge/Maturity-Sandbox-yellow" alt="Maturity Sandbox">
  </a>

<a href="https://github.com/ecmwf/plume-plugin-extreme-events/actions/workflows/ci.yml">
    <img src="https://github.com/ecmwf/plume-plugin-extreme-events/actions/workflows/ci.yml/badge.svg" alt="CI Status">
  </a>

<a href="https://codecov.io/gh/ecmwf/plume-plugin-extreme-events">
    <img src="https://codecov.io/gh/ecmwf/plume-plugin-extreme-events/branch/develop/graph/badge.svg" alt="Code Coverage">
  </a>

  <a href="https://opensource.org/licenses/apache-2-0">
    <img src="https://img.shields.io/badge/License-Apache%202.0-blue.svg" alt="License: Apache 2.0">
  </a>

  <a href="https://github.com/ecmwf/plume-plugin-extreme-events/releases">
    <img src="https://img.shields.io/github/v/release/ecmwf/plume-plugin-extreme-events?color=blue&label=Release&style=flat-square" alt="Latest Release">
  </a>
  <a href="https://plume_extreme_event_detection_plugin.readthedocs.io/en/latest/?badge=latest">
    <img src="https://readthedocs.org/projects/plume_extreme_event_detection_plugin/badge/?version=latest" alt="Documentation Status">
  </a>
</p>

<p align="center">
  <a href="#installation">Installation</a> •
  <a href="#run-with-emulator">Example usage</a> •
  <a href="#contributors">Contributors</a> •
  <a href="https://plume_extreme_event_detection_plugin.readthedocs.io/en/latest/">Documentation</a>
</p>

# Plume Extreme Event Detection Plugin

> \[!IMPORTANT\]
> This software is **Sandbox** and subject to ECMWF's guidelines on [Software Maturity](https://github.com/ecmwf/codex/raw/refs/heads/main/Project%20Maturity).

This Plume plugin is designed to manage a collection of extreme event detection algorithms, and run them at each model step depending on a configuration.
At each time step, this plugin *can* (if configured to do so) send notifications to an Aviso server; each notification should contain enough data for a downstream user to trigger other workflows (e.g., nature of the event, geographical extent, how to write a request to retrieve boundary conditions at relevant steps, fields etc.)

Its initial use case was *wind farm siting and operations*, therefore, the first events to be implemented were selected around the interests of wind farm planners and operators, in the context of the [DTWO](https://dtwo-project.eu/) project.

> [!NOTE]
> It is important to note that the use of the notion of "extreme event" does not refer to an official classification of meteorological events.
> This plugin is intended to be flexible and reusable across different value chains. Therefore, it lies with its users to define what they define as extreme events.
> For instance, prolonged periods of low winds are extreme from the perspective of wind farm and grid operators, as it suggests a power production downtime, but not from the perspective of meteorologists. 


# Features

- **Core plugin component**: the plugin has two main phases:
  - **Setup**
    - Create extreme event instances from the configuration
    - Set up a coarsening matrix to map single grid points to a HEALPix cell. The HEALPix resolution can be configured. It was chosen because it can represent simple or complex regions if nesting is enabled (not supported for now), and a polygon made of HEALPix cells can be represented by a geohash which Aviso may support in the future, thus avoiding the need for polygon building.
  - **Run**
    - Iterate through all the extreme event instances and run their detection method.
    - Extract HEALPix polygons from the detection result.
    - Send notifications to Aviso with all the relevant data.
- **Extreme event registry**: extreme event objects share the same interface for detection. Each event has its own requirements and options, which are explained in the [regristry README](src/ee_registry/README.md).
A registry can be used by the plugin core to construct all the extreme events requested in the configuration.

Example plugin configuration (see registry README for option details on each registered extreme event):

```yaml
plugins:
  - name: "EEPlugin"
    lib: "extreme_event_plugin"
    parameters:
      - &extreme_wind
        - name: "100u"
          type: "atlas_field"
        - name: "100v"
          type: "atlas_field"
    core-config:
        aviso_url: "<url/to/aviso/server>"
        notify_endpoint: "/notify/endpoint"
        enable_notification: true
        healpix_res: 16
        events:
          - name: "extreme_wind"
            enabled: true
            required_params: *extreme_wind
            instances:
              - lower_bound: 25.0
                upper_bound: 0.0
                description: "Extremely strong wind"
```

# Installation

### Requirements

Build dependencies:

- C/C++ compiler (C++17)
- CMake >= 3.16 --- For use and installation see http://www.cmake.org/
- ecbuild >= 3.5 --- ECMWF library of CMake macros (https://github.com/ecmwf/ecbuild)

Runtime dependencies:
  - eckit >= 1.28.5 (https://github.com/ecmwf/eckit)
  - Atlas >= 0.41.0 (https://github.com/ecmwf/atlas)
  - Plume >= 0.2.0  (https://github.com/ecmwf/plume)

### Install

Plume Extreme Event Detection Plugin employs an out-of-source build/install based on CMake.
Make sure ecbuild is installed and the ecbuild executable script is found ( `which ecbuild` ).
Now proceed with installation as follows:

```bash
# Environment --- Edit as needed
srcdir=$(pwd)
builddir=build
installdir=$HOME/.local

# 1. Create the build directory:
mkdir $builddir
cd $builddir

# 2. Run CMake
ecbuild --prefix=$installdir -- \
  -Deckit_ROOT=<path/to/eckit/install> \
  -Datlas_ROOT=<path/to/atlas/install> \
  -Dplume_ROOT=<path/to/plume/install> $srcdir

# 3. Compile / Install
make -j10
make install

# 4. Check installation
$installdir/bin/plume_extreme_event_detection_plugin-version
```

### Run with emulator

Once installed, this plugin can easily be run with the [Plume emulator](https://github.com/ecmwf/plume/blob/develop/src/nwp_emulator/README.md) either from a configuration or from GRIB files (see emulator README). A script wraps the call to the emulator to allow you to provide plugin-specific options. Example usage:

```bash
bash <exec_bin>/emulate_ee_detection.sh --dev --np=8 --expver="0002" --config-src=<path/to/emulator_config.yml> --plume-cfg=<path/to/plume_config.yml>
```

# Contributors

Thank you to all the wonderful people who have contributed to the Extreme Event Detection Plume plugin.
Contributions can come in many forms, including code, documentation, bug reports, feature suggestions, design, and more. A list of code-based contributors can be found [here](https://github.com/ecmwf/plume-plugin-extreme-events/graphs/contributors).

Significant contributions have been made by [ECMWF](https://www.ecmwf.int/) and [DTWO](https://dtwo-project.eu/) project partners.

### How to contribute

Please see the [read the docs](https://plume_extreme_event_detection_plugin.readthedocs.io/en/latest/dev/contributing.html).

## License

See [LICENSE](LICENSE)

## Copyright

© 2025 ECMWF. All rights reserved.
