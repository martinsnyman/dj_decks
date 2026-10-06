#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

// Lightweight waveform preview with a moving playhead marker.
class WaveformDisplay : public Component,
                        public ChangeListener
{
public:
    WaveformDisplay(AudioFormatManager& formatManagerToUse,
                    AudioThumbnailCache& cacheToUse);
    ~WaveformDisplay();

    // Paint waveform or placeholder text when no file is loaded.
    void paint(Graphics&) override;

    // Standard JUCE resize callback (no child components here).
    void resized() override;

    // Repaint when thumbnail data changes.
    void changeListenerCallback(ChangeBroadcaster* source) override;

    // Load a file into waveform thumbnail renderer.
    void loadURL(URL audioURL);

    // Set relative playhead [0..1] for cursor rendering.
    void setPositionRelative(double pos);

private:
    AudioThumbnail audioThumb;
    bool fileLoaded;
    double position;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
