## Known issues and improvements:
### Issues:
- Only left channel is analyzed, maybe conversion to mono would be better

### Possible improvements:
- Send Frequency and Value of highest peaks via OSC, i.e. for external visualization

# Changes:
## v0.4.2:
- Completely new, asymmetric, more accurate window calculation
- Assertion, thrown when opening DAW fixed
- Fixed issue, where scale is resetted when UI is closed and opened again
- Change in tuner frequency doesn't restart whole thread anymore

## v0.4.1:
- New designed, more accurate tuner algorithm
- Fixed frequency scale

## v0.4.0:
- Added frequency scale for more exact analysis by eye

## v0.3.2:
- Fixed bug in CQTThread, now fully artifact-free

## v0.3.0:
- Now in IEM-Design
- Completely scaleable
- OSC Connectivity for receiving parameters

## v0.2.0
- Implementation of tuner algorithm

## v0.1.0:
- First working prototype
