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

#include "CQTThread.h"

CQTThread::Params::Params (const double fs, const float fMin, const float nOctaves, const float B, const float gamma, const float gainInDecibels, const float tuningFreq)
{
    sampleRate = fs;
    bandwidth_max = 0.0f;
    gainFactor = (nOctaves * nOctaves * nOctaves) / 20;  // empirical, can surely be improved
    tuning = tuningFreq;
    
    // Gain for Visualtization
    gainLinear = pow (10, gainInDecibels / 20);

    const double Q = 1.0 / (exp2 (1.0 / B) - exp2 (-1.0 / B));
    const double alpha = 1 / Q;

    const auto m = exp2 (1.0 / B);
    frequencyRatio = m;

    const float fMax = jmin (fs / 2 / m, fMin * pow (2, nOctaves));

    K = roundToInt (std::floor (std::log2 (fMax / fMin) * B));
    binsPerSemitone = int(B / 12.0);

    int Nk = 0;  // current number of samples for each frequencybin
    int Nk_max = 0;  // maximum number of samples for a frequency bin
    
    bandwidth.resize (K);
    
    frequencies.resize (K);
    
    float minOffset = 100.0f;
    
    for (int k = 0; k < K; ++k)
    {
        if (k == 0)
            frequencies[k] = fMin;
        else
            frequencies[k] = frequencies[k - 1] * m;
        
        // TODO: SOFTCODE
        if (fabs (frequencies[k] - tuningFreq) < minOffset)
        {
            nearestBinToTuning = k;
            minOffset = fabs (frequencies[k] - tuningFreq);
        }
        
        //bandwidth
        bandwidth[k] = alpha * frequencies[k] + gamma;
        
        Nk = ceil (round (fs / fMin) * (frequencies[k] / bandwidth[k]));  // calculate number of samples for this center-frequency
        
        if (Nk > Nk_max)
            Nk_max = Nk;
        
        if (bandwidth[k] > bandwidth_max)
            bandwidth_max = bandwidth[k];
    }
    
    
    blockLength = nextPowerOfTwo (ceil (Nk_max));
    fftSize = (fftOversampling * blockLength);
    fftOrder = log2 (fftSize);
    
    df = fs / fftSize;
    
    ifftSize = nextPowerOfTwo (bandwidth_max / df);
    ifftOrder = log2 (ifftSize);
    
    // TODO: currently only working with an overlap of 0.5
    float overlapFactor = 0.5;
    
    overlap = round (overlapFactor * blockLength);
    hopsize = ifftSize / fftOversampling / 2;
    
    std::cout << "Overlapfactor: " << overlapFactor << "\n";
    std::cout << "Overlap in samples: " << overlap << "\n";
    std::cout << "Hopsize: " << hopsize << "\n";
}


CQTThread::CQTThread (const double fs, const float fMin, const float nOctaves, const float B, const float gamma, const float gainInDecibels, const float tuningFreq) :
Thread ("CQT Thread"),
params (fs, fMin, nOctaves, B, gamma, gainInDecibels, tuningFreq),
audioBufferFifo (params.blockLength, numberOfBuffersInQueue),
collector (audioBufferFifo, params.overlap), // second params is overlap (in samples)
fft (params.fftOrder),
ifft (params.ifftOrder),
cqtFifo (params.K, numberOfBuffersInCQTQueue)
{
    fftData.resize (2 * params.fftSize);
    ifftDataComplex.resize (params.ifftSize);
    ifftDataAbs.resize (params.ifftSize);

    cqtBuffer.setSize (params.K, params.ifftSize);
    cqtCollectorBuffer.resize (params.K);
    
    integratedTuning = tuningFreq;
    
    computeWindows();

    startThread (5);
}


CQTThread::~CQTThread()
{
    signalThreadShouldExit();
    stopThread (1000);
}



void CQTThread::computeWindows()
{
    // Window for each sample-block --> in time domain
    hannWindowForTimedomain.resize (params.blockLength);
    WindowingFunction<float>::fillWindowingTables (hannWindowForTimedomain.data(), params.blockLength, WindowingFunction<float>::hann, false);
    
    // hann windows filterbank
    windows.resize (params.K);
    const double df = params.sampleRate / params.fftSize;
    
    for (int k = 0; k < params.K; ++k)
    {
        const float fc = params.frequencies[k];
        // const float r = params.frequencyRatio;
        
        const int firstBin = round ((fc - (params.bandwidth[k] / 2)) / df);
        const int lastBin = round ((fc + (params.bandwidth[k] / 2)) / df);
        const int centerBin = round (fc / df) - firstBin;

        const int winLength = lastBin - firstBin;

        windows[k] = std::make_unique<WindowWithPosition> (firstBin, centerBin);
        windows[k]->resize (winLength);
        
        
        // New window calculation, not so efficient but way more accurate
        std::vector<float> hannInFrequencyDomain (winLength);
        WindowingFunction<float>::fillWindowingTables (hannInFrequencyDomain.data(), winLength, WindowingFunction<float>::hann, false);

        for (int ii = 0; ii < winLength; ++ii)
            windows[k]->operator[] (ii) = hannInFrequencyDomain[ii];
        
        // fft normalization
        FloatVectorOperations::multiply (windows[k]->data(), 1.0f / params.fftSize, static_cast<int> (windows[k]->size()));
    }

    DBG("Windows calculated");
}


void CQTThread::pushSamples (const float* data, int numSamples)
{
    collector.process (data, numSamples);
    notify();
}


void CQTThread::run()
{
    while (! threadShouldExit())
    {
        if (! audioBufferFifo.dataAvailable())
            wait (5);
        else // do processing
        {
            // pop next buffer from  queue
            for (int ii = 0; ii < fftData.size(); ++ii)
                fftData.data()[ii] = NULL;
            
            audioBufferFifo.pop (fftData.data());
 
            // windowing each block in time-domain for smoother edges
            for (int ii = 0; ii < params.blockLength; ++ii)
            {
                fftData.data()[ii] = fftData.data()[ii] * hannWindowForTimedomain[ii] * params.gainLinear;
            }
            
            // Zeropadding --> so in this case circshift
            // std::rotate (fftData.begin(), fftData.end() - int((params.fftOversampling - 1) * params.blockLength * 0.5), fftData.end());
            
            // RFFT
            fft.performRealOnlyForwardTransform (fftData.data(), false);
            
            // Iteration for each CQT-bin
            for (int k = 0; k < params.K; ++k)
            {
                int winLen = static_cast<int> (windows[k]->size());
                fftWindowed.resize (2 * winLen);
                
                for (int ii = 0; ii < winLen; ++ii)
                {
                    fftWindowed[2 * ii] = fftData[2 * (ii + windows[k]->position)] * windows[k]->data()[ii];  // storing in buffer and applying window - real part
                    fftWindowed[2 * ii + 1] = fftData[2 * (ii + windows[k]->position) + 1] * windows[k]->data()[ii];  // imaginary part
                }

                //circshift
                std::rotate (fftWindowed.begin(), fftWindowed.begin() + 2 * round(winLen * 0.5), fftWindowed.end());
                
                // Cast from interleaved to complex
                for (int ii = 0; ii < params.ifftSize; ++ii)
                {
                    if (ii < winLen)
                    {
                        ifftDataComplex.data()[ii] = std::complex<float>(fftWindowed[2 * ii], fftWindowed[2 * ii + 1]);
                    }
                    else
                    {
                        ifftDataComplex.data()[ii] = 0;
                    }
                }
                

                // IFFT
                ifft.perform (ifftDataComplex.data(), ifftDataComplex.data(), true);
                
                for (int ii = 0; ii < params.ifftSize; ++ii)
                    ifftDataAbs.data()[ii] = std::abs(ifftDataComplex.data()[ii]);
                
                
                for (int ii = 0; ii < params.ifftSize; ++ii)
                    cqtBuffer.addSample (k, ii, (params.fftSize/params.ifftSize * params.gainFactor * ifftDataAbs.data()[ii]));
            }
            
            bool activationIdx = 0;
            
            // Setting samples and pushing into cqt-visualizer-FIFO
            for (int ii = 0; ii < params.hopsize; ++ii)
            {
                for (int k = 0; k < params.K; ++k)
                {
                    cqtCollectorBuffer[cqtCollectorBuffer.size() - k - 1] = cqtBuffer.getSample (k, ii);
                    
                    if ((cqtCollectorBuffer[cqtCollectorBuffer.size() - k - 1] > 0.25) && params.binsPerSemitone > 2)
                        activationIdx = true;
                }
                cqtFifo.push (cqtCollectorBuffer.data(), params.K);
                
                if (activationIdx == true)
                    calculateTuning();
            }
            
            
            // shifting cqtBuffer
            for (int ii = 0; ii < params.ifftSize - params.hopsize; ++ii)
            {
                for (int k = 0; k < params.K; ++k)
                {
                    cqtBuffer.setSample (k, ii, cqtBuffer.getSample (k, ii + params.hopsize));
                }
            }
            
            // clearing last entries of cqtBuffer
            for (int ii = params.ifftSize - params.hopsize; ii < params.ifftSize; ++ii)
            {
                for (int k = 0; k < params.K; ++k)
                {
                    cqtBuffer.setSample (k, ii, 0);
                }
            }
        }
    }
}

void CQTThread::calculateTuning()
{
    ++tuningIterationCounter;
    std::reverse(cqtCollectorBuffer.begin(), cqtCollectorBuffer.end());

    int tuningBin = params.nearestBinToTuning;
    
    // start from here if maximum isnt at the tuning-bin
    startTuning:

    
    int modTuning = tuningBin % params.binsPerSemitone;
    
    std::vector<float> summedCqt (3, 0.0f);

    
    for (int ii = 0; ii < params.K; ++ii)
    {
        // summed at tuning bin
        if (ii % params.binsPerSemitone == modTuning){
            summedCqt[1] += cqtCollectorBuffer[ii];
        }
        
        // summed above tuning bin
        if ((ii % params.binsPerSemitone) == ((modTuning + 1) % params.binsPerSemitone)){
            summedCqt[2] += cqtCollectorBuffer[ii];
        }

        // summed below tuning-bin
        if ((ii % params.binsPerSemitone) == ((modTuning - 1 + params.binsPerSemitone) % params.binsPerSemitone)){
            summedCqt[0] += cqtCollectorBuffer[ii];
        }
    }
    
    // shifting tuning-center if above or below the next bin
    if (summedCqt[0] > summedCqt[1])
    {
        tuningBin -= 1;
        goto startTuning;
    }
    else if (summedCqt[2] > summedCqt[1])
    {
        tuningBin += 1;
        goto startTuning;
    }
    
    // caluclate frequency offset as fractual-bin
    float frequencyOffset = 0.5 * (summedCqt[0] - summedCqt[2])/(summedCqt[0] + summedCqt[2] - 2 * summedCqt[1]);
    
    // calculate actual tuning frequency
    float newTuning = params.frequencies[0] * exp2 ((tuningBin + frequencyOffset)/(12 * params.binsPerSemitone));
    
    
    // some sort of integration to smoothen the results
    if (tuningIterationCounter < maxTuningCounter)
        integratedTuning = newTuning/tuningIterationCounter + integratedTuning * (tuningIterationCounter - 1)/tuningIterationCounter;
    else
        integratedTuning = newTuning/maxTuningCounter + integratedTuning * (maxTuningCounter - 1)/maxTuningCounter;

    // conversion to cent
    detuningCents = 1200 * log2 (integratedTuning/params.tuning);
    
    if (detuningCents > 50.0)
        detuningCents -= 50.0;
    else if (detuningCents < -50.0)
        detuningCents += 50.0;
    
    // DBG (detuningCents);
}
