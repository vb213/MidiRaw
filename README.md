#  CQT Analyzer
## Overview
In this free and Open-Source audio plug-in, a real-time capable, efficient FFT based CQT Analyzer is implemented. A visualizer graphically shows the input signal in a time-frequency plot. Also a simple but robust CQT-based tuner is integrated.


## Compilation Guide
All you need for compiling the Plug-in is the [JUCE framework](https://juce.com) with version 5.4.5, an IDE (eg. Xcode, Microsoft Visual Studio), and the [fftw3 library](http://fftw.org).

- Clone/download the repository
- Install and add the fftw3 library (not necessary for macOS)
- Open the .jucer-file with the Projucer (part of JUCE)
- Set your global paths within the Projucer
- If necessary: add additional exporters for your IDE
- Save the project to create the exporter projects
- Open the created projects with your IDE
- Build
- Enjoy ;-)

The *.jucer project is configured to build VST2, VST3 and standalone versions. In order to build the VST2 versions of the plug-ins, you need to have a copy of the Steinberg VST2-SDK which no longer comes with JUCE. 

###  JACK support
Both on macOS and linux, the plug-in standalone version will be built with JACK support. You can disable the JACK support by adding `DONT_BUILD_WITH_JACK_SUPPORT=1` to the *Preprocessor Definitions*-field in the Projucer projects.

## Related repositories
- https://git.iem.at/audioplugins/IEMPluginSuite: a powerful, Open-Source toolbox of Ambisonics-Plugins. The UI is taken from this repository.
