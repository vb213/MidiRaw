CQT transformation

- midi translation
  - parameters such as threshhold, output block granularity
  - output at most 6
- bass only

LATER IDEAS

- cpu saver mode, where not all blocks are processed
- played chord shall be displayed
- scale save mode filtering out off scale notes

now prompt:
transform this plugin into a audio-to-midi translator.

- add a new class which takes the frequency spectrum Bufferqueue from the CQT thread and translates it into midi notes - the class maintains a boolean for each midi note, indicating if it is played currently - translating the spectrum into midi notes means mapping all bins with energy over a certain, adjustable threshhold, to the corresponding midi note and setting the corresponding boolean to true - if a note is not played anymore, set its boolean to false - every on/off change of a midi note should be send to the midi output of the plugin - parameters should be adjustable in the gui
  Yet to be determined:
- exact data flow. who holds the instance of the midi-translator class? make suggestions
  - for this, keep in mind that in the later process (not now!), more midi functionalities shall be added, such as
    - only letting the bass not pass
    - chord recognition

first test:

- single note played -> entire accord of base and overtones sounds
- refine note selection and distinguishing between played notes and overtones
  - just walk "raw" midi spectrum with tone+overtones scheme and only accept note if scheme fits
- latency too long
- more bins seem to improve quality
- low notes tricky
- strums trigger all notes
  Next Steps:
- highlight recognized notes in the Visualizer and write them onto GUI

- make latency lower through what?
