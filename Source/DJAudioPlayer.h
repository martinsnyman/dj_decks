#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include <array>
#include <atomic>

// Audio engine for one deck: playback transport, speed, gain, position and EQ.
class DJAudioPlayer : public AudioSource {
  public:
    DJAudioPlayer(AudioFormatManager& _formatManager);
    ~DJAudioPlayer();

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // Load a new audio file into this player.
    void loadURL(URL audioURL);

    // Set output gain in [0..1].
    void setGain(double gain);

    // Set playback speed ratio in [0.1..4.0].
    void setSpeed(double ratio);

    // Set absolute position in seconds.
    void setPosition(double posInSecs);

    // Set relative position in [0..1] of track length.
    void setPositionRelative(double pos);

    // Set low/mid/high EQ gains in dB for this deck.
    void setEQGains(float lowDb, float midDb, float highDb);

    // Return low/mid/high EQ gains in dB for persistence/UI restore.
    std::array<float, 3> getEQGains() const;

    // Start and stop playback.
    void start();
    void stop();

    // Get current playhead position as relative [0..1].
    double getPositionRelative();

    // Get current track length in seconds.
    double getLengthInSeconds() const;

    // Get current absolute playhead position in seconds.
    double getCurrentPositionInSeconds() const;

private:
    // Rebuild IIR coefficients in place (no allocation). Only call while audio
    // is not being processed concurrently, i.e. from prepareToPlay or the audio thread.
    void updateEqCoefficients();

    AudioFormatManager& formatManager;
    std::unique_ptr<AudioFormatReaderSource> readerSource;
    AudioTransportSource transportSource;
    ResamplingAudioSource resampleSource{ &transportSource, false, 2 };

    // Three-band EQ chain (low shelf -> mid peak -> high shelf).
    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;
    IIRFilter lowShelfFilter;
    IIRFilter midPeakFilter;
    IIRFilter highShelfFilter;

    // EQ values are written by the UI thread and read by the audio thread,
    // which rebuilds coefficients when eqDirty is set.
    double preparedSampleRate{ 44100.0 };
    std::atomic<float> lowEqDb{ 0.0f };
    std::atomic<float> midEqDb{ 0.0f };
    std::atomic<float> highEqDb{ 0.0f };
    std::atomic<bool> eqDirty{ false };
};
