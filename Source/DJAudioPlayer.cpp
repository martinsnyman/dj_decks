#include "DJAudioPlayer.h"

DJAudioPlayer::DJAudioPlayer(AudioFormatManager& _formatManager)
    : formatManager(_formatManager)
{
    // Give the filters valid flat coefficients before audio starts.
    updateEqCoefficients();
}

DJAudioPlayer::~DJAudioPlayer()
{
}

void DJAudioPlayer::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    // Prepare transport + resampler for realtime playback.
    preparedSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);
    transportSource.prepareToPlay(samplesPerBlockExpected, preparedSampleRate);
    resampleSource.prepareToPlay(samplesPerBlockExpected, preparedSampleRate);

    // Prepare DSP EQ chain using current channel/sample settings.
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = preparedSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(1, samplesPerBlockExpected));
    spec.numChannels = 2;

    lowShelfFilter.prepare(spec);
    midPeakFilter.prepare(spec);
    highShelfFilter.prepare(spec);

    updateEqCoefficients();
}

void DJAudioPlayer::getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill)
{
    // Pull audio from transport/resampler first.
    resampleSource.getNextAudioBlock(bufferToFill);

    // Apply any EQ change made by the UI since the last block.
    if (eqDirty.exchange(false))
    {
        updateEqCoefficients();
    }

    if (bufferToFill.buffer == nullptr || bufferToFill.numSamples <= 0)
    {
        return;
    }

    // Apply the tri-band EQ on the generated audio block.
    juce::dsp::AudioBlock<float> fullBlock(*bufferToFill.buffer);
    auto block = fullBlock.getSubBlock(
        static_cast<size_t>(bufferToFill.startSample),
        static_cast<size_t>(bufferToFill.numSamples));
    juce::dsp::ProcessContextReplacing<float> context(block);

    lowShelfFilter.process(context);
    midPeakFilter.process(context);
    highShelfFilter.process(context);
}

void DJAudioPlayer::releaseResources()
{
    // Release resources in reverse order of use.
    transportSource.releaseResources();
    resampleSource.releaseResources();
}

void DJAudioPlayer::loadURL(URL audioURL)
{
    // Build a reader from the selected source and connect it to transport.
    auto* reader = formatManager.createReaderFor(audioURL.createInputStream(
        URL::InputStreamOptions(URL::ParameterHandling::inAddress)));
    if (reader != nullptr)
    {
        std::unique_ptr<AudioFormatReaderSource> newSource(new AudioFormatReaderSource(reader, true));
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
    }
}

void DJAudioPlayer::setGain(double gain)
{
    // Clamp and apply safe gain range.
    transportSource.setGain(juce::jlimit(0.0, 1.0, gain));
}

void DJAudioPlayer::setSpeed(double ratio)
{
    // Keep ratio in a musically useful range to avoid invalid resampling.
    resampleSource.setResamplingRatio(juce::jlimit(0.1, 4.0, ratio));
}

void DJAudioPlayer::setPosition(double posInSecs)
{
    // Move playhead to requested absolute time.
    transportSource.setPosition(juce::jmax(0.0, posInSecs));
}

void DJAudioPlayer::setPositionRelative(double pos)
{
    // Convert relative [0..1] position to seconds if length is known.
    const auto safePos = juce::jlimit(0.0, 1.0, pos);
    const auto lengthSecs = getLengthInSeconds();
    if (lengthSecs > 0.0)
    {
        setPosition(lengthSecs * safePos);
    }
}

void DJAudioPlayer::setEQGains(float lowDb, float midDb, float highDb)
{
    // Store the knob values; the audio thread rebuilds coefficients on its next block.
    lowEqDb = juce::jlimit(-24.0f, 24.0f, lowDb);
    midEqDb = juce::jlimit(-24.0f, 24.0f, midDb);
    highEqDb = juce::jlimit(-24.0f, 24.0f, highDb);
    eqDirty = true;
}

std::array<float, 3> DJAudioPlayer::getEQGains() const
{
    // Expose current EQ values for UI sync and save/load state.
    return { lowEqDb.load(), midEqDb.load(), highEqDb.load() };
}

void DJAudioPlayer::start()
{
    // Start transport playback.
    transportSource.start();
}

void DJAudioPlayer::stop()
{
    // Stop transport playback.
    transportSource.stop();
}

double DJAudioPlayer::getPositionRelative()
{
    // Guard against divide-by-zero when no track is loaded.
    const auto length = getLengthInSeconds();
    if (length <= 0.0)
    {
        return 0.0;
    }

    return transportSource.getCurrentPosition() / length;
}

double DJAudioPlayer::getLengthInSeconds() const
{
    // Ask transport for current file length.
    return transportSource.getLengthInSeconds();
}

double DJAudioPlayer::getCurrentPositionInSeconds() const
{
    // Ask transport for current playhead time.
    return transportSource.getCurrentPosition();
}

void DJAudioPlayer::updateEqCoefficients()
{
    // Use standard DJ-ish crossover points: low shelf 200Hz, high shelf 4kHz.
    const auto sr = juce::jmax(8000.0, preparedSampleRate);

    using ArrayCoefficients = juce::dsp::IIR::ArrayCoefficients<float>;

    // Write into the existing coefficient objects so no memory is freed or allocated
    // while the filters may be reading them.
    *lowShelfFilter.state = ArrayCoefficients::makeLowShelf(
        sr,
        200.0,
        0.70710678,
        juce::Decibels::decibelsToGain(lowEqDb.load()));

    *midPeakFilter.state = ArrayCoefficients::makePeakFilter(
        sr,
        1000.0,
        1.0,
        juce::Decibels::decibelsToGain(midEqDb.load()));

    *highShelfFilter.state = ArrayCoefficients::makeHighShelf(
        sr,
        4000.0,
        0.70710678,
        juce::Decibels::decibelsToGain(highEqDb.load()));
}

