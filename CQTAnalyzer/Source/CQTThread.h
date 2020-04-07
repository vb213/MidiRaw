/*
==============================================================================
This file is part of the IEM plug-in suite.
Author: Felix Holzmüller
Copyright (c) 2020 - Institute of Electronic Music and Acoustics (IEM)
https://iem.at

The IEM plug-in suite is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

The IEM plug-in suite is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this software.  If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "Utilities/BufferQueue.h"
#include "Utilities/OverlappingSampleCollector.h"

#include <algorithm>  // for rotation/circshift
#include <math.h>
#include <vector>
#include <climits>

using namespace dsp;

/** This class computes the Constant-Q Transform of incoming audio samples, which are buffered using a Fifo. The class starts a thread, which does all of the computation, and outputs the results into another Fifo. A CQTThread object is reference-counted. So you can easily set up another instance of it and overwrite the pointer of the previous one. Make sure you create a copy of the pointer before you call any methods, to ensure the object its members won't get deleted during processing.
 */
class CQTThread  :  public ReferenceCountedObject, public Thread
{
    /** Helperclass which computes and holds the necessary parameters.
     */
    struct Params
    {
        Params (const double fs, const float fMin, const float nOctaves, const float B, const float gamma, const float tuningFreq);
        static constexpr int fftOversampling = 2;
        double sampleRate;
        int fftOrder, fftSize;
        int K;
        std::vector<float> frequencies;
        std::vector<float> B_gammacorrected;
        int ifftOrder, ifftSize;
        int blockLength;
        int hopsize;
        float df;
        float bandwidth_max;
        float gainFactor;
        int overlap;
        int binsPerSemitone;
        int nearestBinToTuning;
        float tuning;
        
    };

    /** Small structure holding a window and a position information.
     */
    struct WindowWithPosition : public std::vector<float>
    {
        WindowWithPosition (const int firstBin) : position (firstBin){}
        const int position;
    };

public:
    static constexpr int numberOfBuffersInQueue = 10;
    static constexpr int numberOfBuffersInCQTQueue = 4096;

    using Ptr = ReferenceCountedObjectPtr<CQTThread>;

    CQTThread (const double fs, const float fMin, const float nOctaves, const float B, const float gamma, const float tuningFreq);
    ~CQTThread();

    /** Writes samples into queue, which will be processed once enough samples are gathered.
     */
    void pushSamples (const float* data, int numSamples);

    /** Returns the BufferQueue used to store the CQT coefficients. Make sure you hold a retained pointer to this reference-counted class, so the queue isn't destroyed during use.
     */
    BufferQueue<float>& getCqtFifo() { return cqtFifo; }
    
    void setTuningFreq( float newTuning ){
        setTuningFlag = true;
        currentTuningFreq = newTuning;
    }
    
    
    float& getTuning() { return detuningCents; }

private:

    /** Computes the necessary windows: Tukey-Window for the input samples, and a bunch of Hann-windows for the spectral filtering.
     */
    void computeWindows();

    /** That's the thread's main routine.
     */
    void run() override;
    
    void calculateTuning();

    Params params;
    float currentTuningFreq = 0.0f;

    BufferQueue<float> audioBufferFifo;
    OverlappingSampleCollector<float> collector;

    //std::vector<float> tukeyWindow;
    
    FFT fft;
    std::vector<float> fftData;

    FFT ifft;
    std::vector<std::complex<float>> ifftData;

    std::vector<std::unique_ptr<WindowWithPosition>> windows;
    
    std::vector<float> hannWindowForTimedomain;

    float integratedTuning;
    float detuningCents = 0.0f;
    int tuningIterationCounter = 0;
    int maxTuningCounter = 128;
    bool setTuningFlag = false;
    
    AudioBuffer<float> cqtBuffer;
    std::vector<float> cqtCollectorBuffer;
    BufferQueue<float> cqtFifo;
};
