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

CQTThread::Params::Params (const double fs, const float fMin, const float nOctaves, const float B, const float gamma)
{
    sampleRate = fs;
    bandwidth_max = 0.0f;
    gainFactor = (nOctaves * nOctaves * nOctaves)/10;  // empirical, can surely be improved

    const double Q = 1.0 / (exp2 (1.0 / B) - exp2 (-1.0 / B));
    const double alpha = 1 / Q;
    
    const float m = static_cast<float> (exp2 (1.0 / B));

    const double fMax = juce::jmin (fs / 2 / m, fMin * pow (2, nOctaves));

    K = static_cast<unsigned int> (juce::roundToInt (std::floor (std::log2 (fMax / fMin) * B)));
    

    int Nk = 0;  // current number of samples for each frequencybin
    int Nk_max = 0;  // maximum number of samples for a frequency bin
    
    std::vector<float> bandwidth (K);
    frequencies.resize (K);
    B_gammacorrected.resize(K);
    
    for (unsigned int k = 0; k < K; ++k)
    {
        
        if (k == 0)
            frequencies[k] = fMin;
        else
            frequencies[k] = frequencies[k - 1] * m;
        
        //bandwidth
        bandwidth[k] = static_cast<float> (alpha * frequencies[k] + gamma);
        
        Nk = static_cast<int> (ceil (round (fs / fMin) * (frequencies[k] / bandwidth[k])));  // calculate number of samples for this center-frequency
        
        if (Nk > Nk_max)
            Nk_max = Nk;
        
        if (bandwidth[k] > bandwidth_max)
            bandwidth_max = bandwidth[k];
        
        B_gammacorrected[k] = static_cast<float> (log (2.0) / asinh (bandwidth[k] / (2 * frequencies[k])));
    }

    
    blockLength = juce::nextPowerOfTwo (static_cast<int>( ceil (Nk_max)));
    fftSize = (fftOversampling * blockLength);
    fftOrder = static_cast<int> (log2 (fftSize));
    
    df = fs / fftSize;
    
    ifftSize = juce::nextPowerOfTwo (juce::roundToInt(bandwidth_max / df));
    ifftOrder = static_cast<int> (log2 (ifftSize));
    
    float overlapFactor = 0.5;
    
    overlap = juce::roundToInt (overlapFactor * blockLength);
    hopsize = ifftSize / fftOversampling / 2;
}


CQTThread::CQTThread (const double fs, const float fMin, const float nOctaves, const float B, const float gamma) :
Thread ("CQT Thread"),
    params (fs, fMin, nOctaves, B, gamma),
audioBufferFifo (params.blockLength, numberOfBuffersInQueue),
collector (audioBufferFifo, params.overlap), // second params is overlap (in samples)
fft (params.fftOrder),
ifft (params.ifftOrder),
cqtFifo (static_cast<int>(params.K), numberOfBuffersInCQTQueue),
    midiFifo (static_cast<int>(params.K), numberOfBuffersInMidiQueue)
{
    fftData.resize (static_cast<unsigned int> (2 * params.fftSize));
    ifftInData.resize (static_cast<unsigned int>(params.ifftSize));
    ifftOutData.resize(static_cast<unsigned int>(params.ifftSize));

    cqtBuffer.setSize (static_cast<int>(params.K), params.ifftSize);
    cqtBuffer.clear();

    cqtCollectorBuffer.resize (params.K);
    
    
    computeWindows();

    startThread (5);
}


CQTThread::~CQTThread()
{
    signalThreadShouldExit();
    stopThread (500);
}



void CQTThread::computeWindows()
{
    // Window for each sample-block --> in time domain
    hannWindowForTimedomain.resize (static_cast<unsigned int>(params.blockLength));
    WindowingFunction<float>::fillWindowingTables (hannWindowForTimedomain.data(), static_cast<unsigned int>(params.blockLength), WindowingFunction<float>::hann, false);
    
    std::vector<float> hannLookup (static_cast<unsigned int>(8 * params.ifftSize + 1)); // maybe a smaller window is already sufficient
    WindowingFunction<float>::fillWindowingTables (hannLookup.data(), hannLookup.size(), WindowingFunction<float>::hann, false);
    const int halfwin_len = static_cast<int>(floor (hannLookup.size() / 2));
    
    windows.resize (params.K);
    const double df = params.sampleRate / params.fftSize;
    
    for (unsigned int k = 0; k < params.K; ++k)
    {
        float fc = params.frequencies[k];
        
        int firstBin = static_cast<int> (ceil (fc * exp2 (-1 / params.B_gammacorrected[k]) / df));
        int lastBin = static_cast<int>(floor (fc * exp2 (1 / params.B_gammacorrected[k]) / df));

        int winLength = lastBin - firstBin + 1;

        // Initialize window-array
        windows[k] = std::make_unique<WindowWithPosition> (firstBin);
        windows[k]->resize (static_cast<unsigned int>(winLength));
        
        // Calculate corresponding frequencies of the lookup-window for the current CQT-bin
        std::vector<float> windowFrequencies (hannLookup.size());
        for (unsigned int jj = 0; jj < hannLookup.size(); ++jj)
            windowFrequencies[jj] = fc * exp2 ((-halfwin_len + static_cast<int>(jj)) / (halfwin_len * params.B_gammacorrected[k]));

        // New window calculation, not so efficient but way more accurate
        for (unsigned int ii = 0; ii < static_cast<unsigned int>(winLength); ++ii)
        {
            std::vector<float> frequencyDifference = windowFrequencies;
            
            // Calculate difference to currently evaluated DFT-bin
            for (unsigned int jj = 0; jj < hannLookup.size(); ++jj)
                frequencyDifference[jj] = static_cast<float> (fabs (windowFrequencies[jj] - (firstBin + static_cast<int>(ii)) * params.df));
            
            // find minimum difference
            const unsigned int win_idx = static_cast<unsigned int> (std::min_element (frequencyDifference.begin(), frequencyDifference.end()) - frequencyDifference.begin());
            
            // Store results in window-array
            windows[k]->operator[] (ii) = hannLookup[win_idx];
        }
        
        // fft normalization
        juce::FloatVectorOperations::multiply (windows[k]->data(), 1.0f / params.fftSize, static_cast<int> (windows[k]->size()));
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
            // Reset FFT-Vector to 0
            for (unsigned int ii = 0; ii < fftData.size(); ii++)
                fftData.data()[ii] = 0.0f;
            
            // pop next buffer from  queue
            audioBufferFifo.pop (fftData.data());
 
            // Apply window in time domain for blockbased processing
            juce::FloatVectorOperations::multiply(fftData.data(), hannWindowForTimedomain.data(), params.blockLength);

            // RFFT
            fft.performRealOnlyForwardTransform(fftData.data(), true);
            
            // Iteration for each CQT-bin
            for (unsigned int k = 0; k < params.K; ++k)
            {
                unsigned int winLen = static_cast<unsigned int> (windows[k]->size());
                
                // conversion to complex and applying window
                for (unsigned int ii = 0; ii < static_cast<unsigned int>(params.ifftSize); ii++)
                {
                    if (ii < winLen)
                        ifftInData[ii] = std::complex<float> (fftData[2 * (ii + windows[k]->position)],
                                                              fftData[2 * (ii + windows[k]->position) + 1]) * windows[k]->data()[ii];
                    else
                        ifftInData[ii] = std::complex<float> (0.0f, 0.0f);

                    ifftOutData[ii] = std::complex<float> (0.0f, 0.0f);
                }
                

                // IFFT
                ifft.perform (ifftInData.data(), ifftOutData.data(), true);

                for (int ii = 0; ii < static_cast<int>(params.ifftSize); ii++)
                    cqtBuffer.addSample(static_cast<int>(k), ii, float(params.fftSize / params.ifftSize * params.gainFactor * std::abs(ifftOutData.data()[ii])));
            }
            
            
            // Setting samples and pushing into cqt-visualizer-FIFO
            for (int ii = 0; ii < params.hopsize; ii++)
            {
                for (unsigned int k = 0; k < params.K; k++)
                {
                    cqtCollectorBuffer[cqtCollectorBuffer.size() - k - 1] = cqtBuffer.getSample (static_cast<int>(k), ii);

                }

                cqtFifo.push (cqtCollectorBuffer.data(), static_cast<int>(params.K));
                midiFifo.push (cqtCollectorBuffer.data(), static_cast<int>(params.K));
            }
            
            
            // shifting cqtBuffer
            for (int ii = 0; ii < params.ifftSize - params.hopsize; ++ii)
                for (int k = 0; k < static_cast<int>(params.K); ++k)
                    cqtBuffer.setSample (k, ii, cqtBuffer.getSample (k, ii + params.hopsize));
            
            // clearing last entries of cqtBuffer
            for (int ii = params.ifftSize - params.hopsize; ii < params.ifftSize; ++ii)
                for (int k = 0; k < static_cast<int>(params.K); ++k)
                    cqtBuffer.setSample (k, ii, 0.0f);

        }
    }
}




