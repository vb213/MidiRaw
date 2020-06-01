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

CQTThread::Params::Params (const double fs, const float fMin, const float nOctaves, const float B, const float gamma, const float tuningFreq) : gammaParam (gamma)
{
    sampleRate = fs;
    bandwidth_max = 0.0f;
    gainFactor = (nOctaves * nOctaves * nOctaves)/10;  // empirical, can surely be improved
    tuning = tuningFreq;
    nearestBinToTuning = 0;

    const double Q = 1.0 / (exp2 (1.0 / B) - exp2 (-1.0 / B));
    const double alpha = 1 / Q;
    
    const auto m = exp2 (1.0 / B);

    const float fMax = jmin (fs / 2 / m, fMin * pow (2, nOctaves));

    K = roundToInt (std::floor (std::log2 (fMax / fMin) * B));
    binsPerSemitone = int(B / 12.0);
    

    int Nk = 0;  // current number of samples for each frequencybin
    int Nk_max = 0;  // maximum number of samples for a frequency bin
    
    std::vector<float> bandwidth (K);
    frequencies.resize (K);
    B_gammacorrected.resize(K);
    
    for (int k = 0; k < K; ++k)
    {
        
        if (k == 0)
            frequencies[k] = fMin;
        else
            frequencies[k] = frequencies[k - 1] * m;
        
        //bandwidth
        bandwidth[k] = alpha * frequencies[k] + gamma;
        
        Nk = ceil (round (fs / fMin) * (frequencies[k] / bandwidth[k]));  // calculate number of samples for this center-frequency
        
        if (Nk > Nk_max)
            Nk_max = Nk;
        
        if (bandwidth[k] > bandwidth_max)
            bandwidth_max = bandwidth[k];
        
        B_gammacorrected[k] = log (2.0) / asinh (bandwidth[k] / (2 * frequencies[k]));
    }

    
    blockLength = nextPowerOfTwo (ceil (Nk_max));
    fftSize = (fftOversampling * blockLength);
    fftOrder = log2 (fftSize);
    
    df = fs / fftSize;
    
    ifftSize = nextPowerOfTwo (bandwidth_max / df);
    ifftOrder = log2 (ifftSize);
    
    float overlapFactor = 0.5;
    
    overlap = round (overlapFactor * blockLength);
    hopsize = ifftSize / fftOversampling / 2;
}


CQTThread::CQTThread (const double fs, const float fMin, const float nOctaves, const float B, const float gamma, const float tuningFreq, const float initialTunerStatus) :
Thread ("CQT Thread"),
params (fs, fMin, nOctaves, B, gamma, tuningFreq),
audioBufferFifo (params.blockLength, numberOfBuffersInQueue),
collector (audioBufferFifo, params.overlap), // second params is overlap (in samples)
fft (params.fftOrder),
ifft (params.ifftOrder),
cqtFifo (params.K, numberOfBuffersInCQTQueue)
{
    fftData.resize (2 * params.fftSize);
    ifftData.resize (params.ifftSize);

    cqtBuffer.setSize (params.K, params.ifftSize);
    cqtBuffer.clear();

    cqtCollectorBuffer.resize (params.K);
    
    setTuningFreq(tuningFreq);
    integratedTuning = tuningFreq;
    tunerStatus = bool (initialTunerStatus);
    
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
    hannWindowForTimedomain.resize (params.blockLength);
    WindowingFunction<float>::fillWindowingTables (hannWindowForTimedomain.data(), params.blockLength, WindowingFunction<float>::hann, false);
    
    std::vector<float> hannLookup (8 * params.ifftSize + 1); // maybe a smaller window is already sufficient
    WindowingFunction<float>::fillWindowingTables (hannLookup.data(), hannLookup.size(), WindowingFunction<float>::hann, false);
    const int halfwin_len = floor (hannLookup.size() / 2);
    
    windows.resize (params.K);
    const double df = params.sampleRate / params.fftSize;
    
    for (int k = 0; k < params.K; ++k)
    {
        float fc = params.frequencies[k];
        
        int firstBin = ceil (fc * exp2 (-1 / params.B_gammacorrected[k]) / df);
        int lastBin = floor (fc * exp2 (1 / params.B_gammacorrected[k]) / df);

        int winLength = lastBin - firstBin + 1;

        // Initialize window-array
        windows[k] = std::make_unique<WindowWithPosition> (firstBin);
        windows[k]->resize (winLength);
        
        // Calculate corresponding frequencies of the lookup-window for the current CQT-bin
        std::vector<float> windowFrequencies (hannLookup.size());
        for (int jj = 0; jj < hannLookup.size(); ++jj)
            windowFrequencies[jj] = fc * exp2 ((-halfwin_len + jj) / (halfwin_len * params.B_gammacorrected[k]));

        // New window calculation, not so efficient but way more accurate
        for (int ii = 0; ii < winLength; ++ii)
        {
            std::vector<float> frequencyDifference = windowFrequencies;
            
            // Calculate difference to currently evaluated DFT-bin
            for (int jj = 0; jj < hannLookup.size(); ++jj)
                frequencyDifference[jj] = fabs (windowFrequencies[jj] - (firstBin + ii) * params.df);
            
            // find minimum difference
            const int win_idx = int (std::min_element (frequencyDifference.begin(), frequencyDifference.end()) - frequencyDifference.begin());
            
            // Store results in window-array
            windows[k]->operator[] (ii) = hannLookup[win_idx];
        }
        
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
    int cycleCounter = 0;
    while (! threadShouldExit())
    {
        if (! audioBufferFifo.dataAvailable())
            wait (5);
        else // do processing
        {
            // Reset FFT-Vector to 0
            for (int ii = 0; ii < fftData.size(); ii++)
                fftData.data()[ii] = 0.0f;
            
            // pop next buffer from  queue
            audioBufferFifo.pop (fftData.data());

            // Testsignal
            for (int ii = 0; ii < params.blockLength; ++ii)
            {
                int periode = 48;
                fftData.data()[ii] = dsp::FastMathApproximations::sin((ii % periode) * MathConstants<float>::twoPi / periode);
            }
 
            // Apply window in time domain for blockbased processing
            FloatVectorOperations::multiply(fftData.data(), hannWindowForTimedomain.data(), params.blockLength);
            //std::vector<std::complex<float>> fftOutput;
            //std::vector<std::complex<float>> fftInput;

            //fftOutput.resize(params.fftSize);
            //fftInput.resize(params.fftSize);

            //for (int ii = 0; ii < params.blockLength; ii++)
            //    fftInput[ii] = std::complex<float>(fftData.data()[ii], 0.0f);


            // RFFT
            //fft.perform (fftInput.data(), fftOutput.data(), false);
            fft.performRealOnlyForwardTransform(fftData.data(), true);
            
            // Iteration for each CQT-bin
            for (int k = 0; k < params.K; ++k)
            {
                int winLen = static_cast<int> (windows[k]->size());
                
                // conversion to complex and applying window
                for (int ii = 0; ii < params.ifftSize; ii++)
                {
                    if (ii < winLen)
                        ifftData[ii] = std::complex<float> (fftData[2 * (ii + windows[k]->position)], fftData[2 * (ii + windows[k]->position) + 1]) * windows[k]->data()[ii];
                        //ifftData[ii] = fftOutput[ii + windows[k]->position] * windows[k]->data()[ii];
                    else
                        ifftData[ii] = 0;
                }
                

                // IFFT
                ifft.perform (ifftData.data(), ifftData.data(), true);
                
                for (int ii = 0; ii < params.ifftSize; ii++)
                    cqtBuffer.addSample (k, ii, (params.fftSize/params.ifftSize * params.gainFactor * std::abs(ifftData.data()[ii])));
            }
            
            bool activationIdx = 0;
            
            // Setting samples and pushing into cqt-visualizer-FIFO
            for (int ii = 0; ii < params.hopsize; ii++)
            {
                for (int k = 0; k < params.K; k++)
                {
                    cqtCollectorBuffer[cqtCollectorBuffer.size() - k - 1] = cqtBuffer.getSample (k, ii);              

                    if ((cqtCollectorBuffer[cqtCollectorBuffer.size() - k - 1] > 0.1) && params.binsPerSemitone > 2)
                        activationIdx = true;
                }
                cqtFifo.push (cqtCollectorBuffer.data(), params.K);
            }
            
            
            // shifting cqtBuffer
            for (int ii = 0; ii < params.ifftSize - params.hopsize; ++ii)
            {
                for (int k = 0; k < params.K; k++)
                {
                    cqtBuffer.setSample (k, ii, cqtBuffer.getSample (k, ii + params.hopsize));
                }
            }
            
            // clearing last entries of cqtBuffer
            for (int ii = params.ifftSize - params.hopsize; ii < params.ifftSize; ii++)
            {
                for (int k = 0; k < params.K; k++)
                {
                    cqtBuffer.setSample (k, ii, 0.0f);
                }
            }

            // Start tuner
            if ((activationIdx == true) && (params.gammaParam < gammaTh) && (tunerStatus == true))
                calculateTuning();
        }
    }
}


void CQTThread::setTuningFreq (const float newTuning)
{
    params.tuning = newTuning;
    
    float minOffset = 100.0f;
    for (int k = 0; k < params.K; k++){
        if (fabs (params.frequencies[k] - params.tuning) < minOffset)
        {
            params.nearestBinToTuning = k;
            minOffset = fabs (params.frequencies[k] - params.tuning);
        }
    }
}


void CQTThread::calculateTuning()
{
    ++tuningIterationCounter;
    std::reverse(cqtCollectorBuffer.begin(), cqtCollectorBuffer.end());
    
    // start from here if maximum isnt at the tuning-bin
    startTuning:

    
    // safety if tuning bin is shifted so often that it is out of bounds
    if ((params.nearestBinToTuning < 2) || ((params.nearestBinToTuning-1) == params.K))
        setTuningFreq (params.tuning);
    
    int modTuning = params.nearestBinToTuning % params.binsPerSemitone;
    
    
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
        params.nearestBinToTuning -= 1;
        goto startTuning;
    }
    else if (summedCqt[2] > summedCqt[1])
    {
        params.nearestBinToTuning += 1;
        goto startTuning;
    }

    // saving frequencies for more compact calculation
    float a = params.frequencies[params.nearestBinToTuning - 1];
    float b = params.frequencies[params.nearestBinToTuning];
    float c = params.frequencies[params.nearestBinToTuning + 1];
    
    // actual calculation based upon parabolic interpolation
    float newTuning = b + 0.5 * ((summedCqt[0] - summedCqt[1]) * pow((c - b), 2) -
                                 (summedCqt[2] - summedCqt[1]) * pow((b - a), 2))/
                                ((summedCqt[0] - summedCqt[1]) * (c - b) +
                                 (summedCqt[2] - summedCqt[1]) * (b - a));
    
    // some sort of integration to smoothen the results
    if (tuningIterationCounter > maxTuningCounter)
        tuningIterationCounter = maxTuningCounter;
    
    integratedTuning = newTuning/tuningIterationCounter + integratedTuning * (tuningIterationCounter - 1)/tuningIterationCounter;

    // conversion to cent
    detuningCents = 1200 * log2 (integratedTuning/params.tuning);

    // Modulo, so the solution is in the range of +- 100
    detuningCents = float (roundToInt (detuningCents) % 100);
    
    // Limiting to +-50
    if (detuningCents > 50.0f)
        detuningCents -= 100.0f;
    else if (detuningCents < -50.0f)
        detuningCents += 100.0f;
}
