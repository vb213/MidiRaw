#  CQT Analyzer

## Overview
In this free and Open-Source audio plug-in, a real-time capable, efficient FFT based CQT Analyzer is implemented. A visualizer graphically shows the input signal in a time-frequency plot. Also, a simple but robust CQT-based tuner is integrated. A short demo-video can be found [here](https://vimeo.com/418146505).

## Features

- Built with [JUCE](https://juce.com/) 7.0.2
- Based on the [IEM Plug-In Suite](https://git.iem.at/audioplugins/IEMPluginSuite)
- Pre-built as VST, VST3 and LV2. The formats AAX and AU are also possible to build
- Universal build for macOS
- OS X 10.9+ *should* be supported (but only tested on macOS 12.6 with Apple Silicon Chip)
- Windows Vista/8/10 or newer *should* be supported (but only tested on Windows 10 21H2)

## Installation Guide
Already compiled VST, VST3 and LV2 plug-ins for macOS and Windows can be found at the releases page. Download the archive and copy the files into the plug-in directory of your system. As the program is not signed, a warning prompt will be shown on Windows. At macOS 10.15+, you have to manually allow the plug-in to run in your system security preferences.

The default paths for plug-ins are:

- VST:
  - macOS: /Library/Audio/Plug-Ins/VST or ~/Library/Audio/Plug-Ins/VST
  - Windows: C:\Programm Files\Steinberg\VstPlugins
  - Linux: /usr/lib/lxvst or /usr/local/lib/lxvst
- VST3:
  - macOS: Library/Audio/Plug-ins/VST3 or ~/Library/Audio/Plug-Ins/VST3
  - Windows: C:\Program Files\Common Files\VST3
  - Linux: /usr/lib/lxvst or /usr/local/lib/lxvst
- LV2:
  - macOS: /Library/Audio/Plug-Ins/LV2 or ~/Library/Audio/Plug-Ins/LV2
  - Windows: %COMMONPROGRAMFILES%/LV2 or %APPDATA%/LV2
  - Linux: /usr/lib/lv2 or $HOME/.lv2

You might need to restart your DAW before being able to use the plug-ins.

## Compilation Guide
All you need for compiling the Plug-in is a recent version of [CMake](https://cmake.org/) and [Git](https://git-scm.com/) as well as a C++ compiler (e.g. bundled with Xcode for macOS or Visual Studio for Windows). JUCE is automatically added as git submodule, so no installation is needed.

- Clone/download the repository
- Open your command line and go to the cloned directory
- Build without IDE
  - Generate a project with ```cmake -B Builds```
  - Compile the project with ```cmake --build Builds```
- Or generate an IDE project
  - Generate a project with ```cmake -B Builds -G "Your Generator"```. To see all possible generators, just type ```cmake -G```
  - Open the project in the ```Build/``` folder and compile the project via your IDE
- The files should be somewhere in ```Builds\CQTAnalyzer_artefacts``` and are also automatically copied to your local plug-in directory
- Enjoy ;-)

The project is configured to build VST3 and Standalone application by default. Other formats can be compiled by settings the flags ```-DIEM_BUILD_VST2=ON```, ```-DIEM_BUILD_AU=ON``` or ```-DIEM_BUILD_LV2=ON``` during the CMake generation. On macOS is by default no universal build generated to speed up development. However, it can be activated with the flag ```-DIEM_MACOS_UNIVERSAL=ON```. In order to build the VST2 versions of the plug-ins, you need to have a copy of the Steinberg VST2-SDK which no longer comes with JUCE.

###  JACK support

Both on macOS and linux, the plug-in standalone version can be built with JACK support. You can enable the JACK support by setting ```-DIEM_STANDALONE_JACK_SUPPORT=ON```.

## Citation
If you used this Plugin for a publication or a thesis, we would be glad if you cite our work:

F. Holzmüller, P. Bereuter, P. Merz, D. Rudrich, and A. Sontacchi, “Computational effective real-time capable constant-Q spectrum analyzer,” in Proceedings of the AES 148th Convention, May 2020, [Online]. Available: http://www.aes.org/e-lib/browse.cfm?elib=20805.

## Related repositories
- https://git.iem.at/audioplugins/IEMPluginSuite: a powerful, Open-Source toolbox of Ambisonics-Plugins. The UI is taken from this repository.
