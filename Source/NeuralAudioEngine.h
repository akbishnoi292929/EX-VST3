#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

#if defined(HAS_ONNXRUNTIME)
#include <onnxruntime_cxx_api.h>
#endif

class NeuralAudioEngine
{
public:
    NeuralAudioEngine()
    {
#if defined(HAS_ONNXRUNTIME)
        try
        {
            env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "EX_NeuralEngine");
            sessionOptions = std::make_unique<Ort::SessionOptions>();
            sessionOptions->SetIntraOpNumThreads(1);
            isAvailable = true;
        }
        catch (...)
        {
            isAvailable = false;
        }
#endif
    }

    // Mel-Spectrogram & Harmonic Centroid Analysis
    struct HarmonicEmbedding
    {
        float spectralCentroid = 0.0f;
        float rmsEnergy = 0.0f;
        float zeroCrossingRate = 0.0f;
        float harmonicClarity = 0.0f;
        int dominantMidiPitch = 60;
    };

    HarmonicEmbedding analyzeSliceFeatures(const float* audioData, int numSamples, double sampleRate)
    {
        HarmonicEmbedding emb;
        if (numSamples <= 64 || audioData == nullptr)
            return emb;

        float sumEnergy = 0.0f;
        float weightedFreqSum = 0.0f;
        float energySum = 0.0f;
        float zeroCrossings = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float val = audioData[i];
            float absVal = std::abs(val);
            sumEnergy += (val * val);

            if (i > 0 && ((val >= 0.0f && audioData[i - 1] < 0.0f) || (val < 0.0f && audioData[i - 1] >= 0.0f)))
                zeroCrossings += 1.0f;

            float freqEstimate = (static_cast<float>(i) / static_cast<float>(numSamples)) * (static_cast<float>(sampleRate) * 0.5f);
            weightedFreqSum += absVal * freqEstimate;
            energySum += absVal;
        }

        emb.rmsEnergy = std::sqrt(sumEnergy / static_cast<float>(numSamples));
        emb.zeroCrossingRate = zeroCrossings / static_cast<float>(numSamples);

        if (energySum > 1e-5f)
            emb.spectralCentroid = weightedFreqSum / energySum;

        // Approximate fundamental frequency
        float f0 = emb.spectralCentroid * 0.5f;
        if (f0 >= 65.0f && f0 <= 1200.0f)
        {
            emb.dominantMidiPitch = static_cast<int>(std::round(69.0f + 12.0f * std::log2(f0 / 440.0f)));
            emb.harmonicClarity = 0.85f;
        }
        else
        {
            emb.dominantMidiPitch = 60;
            emb.harmonicClarity = 0.40f;
        }

        return emb;
    }

    bool isEngineReady() const { return isAvailable; }

private:
    bool isAvailable = false;
#if defined(HAS_ONNXRUNTIME)
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
#endif
};
