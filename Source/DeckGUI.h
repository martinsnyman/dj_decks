#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "DJAudioPlayer.h"
#include "WaveformDisplay.h"
#include <array>
#include <functional>
#include <memory>

// Deck UI for one player: transport controls, hot cues, EQ and status widgets.
class DeckGUI : public Component,
                public Button::Listener,
                public Slider::Listener,
                public FileDragAndDropTarget,
                public Timer
{
public:
    DeckGUI(DJAudioPlayer* player,
            AudioFormatManager& formatManagerToUse,
            AudioThumbnailCache& cacheToUse,
            const juce::String& deckTitle);
    ~DeckGUI();

    void paint(Graphics&) override;
    void resized() override;

    // Handle transport, file, hot cue and clear-cues button actions.
    void buttonClicked(Button*) override;

    // Handle gain/speed/position and EQ slider changes.
    void sliderValueChanged(Slider* slider) override;

    // Accept dragged files to load directly into this deck.
    bool isInterestedInFileDrag(const StringArray& files) override;
    void filesDropped(const StringArray& files, int x, int y) override;

    // Refresh waveform and transport indicator periodically.
    void timerCallback() override;

    // Load a specific track from library metadata and persisted hot cues.
    void loadTrack(const juce::File& audioFile,
                   const juce::String& trackId,
                   const juce::String& trackName,
                   double bpm,
                   const std::array<double, 8>& cues);

    // Restore and display persisted EQ values for this deck.
    void setEqValues(float lowDb, float midDb, float highDb);

    // Callback when user loads a new file directly from this deck.
    std::function<void(const juce::File&)> onTrackLoaded;

    // Callback for track-based hot cue updates (set, overwrite or clear).
    std::function<void(const juce::String&, int, double)> onHotCueChanged;

    // Callback for per-deck EQ value changes.
    std::function<void(float, float, float)> onEqChanged;

private:
    // Helper to load track file and optional metadata into UI/player.
    void applyTrackLoad(const juce::File& audioFile,
                        const juce::String& newTrackId,
                        const juce::String& trackName,
                        double bpm,
                        const std::array<double, 8>* cuesToApply);

    // Helper to refresh hot cue button captions after cue edits.
    void updateHotCueButtonTexts();

    // Helper to set a single hot cue to current playhead location.
    void assignHotCue(int cueIndex);

    // Helper to jump to an existing hot cue location.
    void triggerHotCue(int cueIndex);

    juce::FileChooser fChooser{ "Select a file..." };

    // Header + transport controls.
    Label deckTitleLabel;
    Label trackInfoLabel;
    Label bpmLabel;
    TextButton playButton{ "PLAY" };
    TextButton stopButton{ "STOP" };
    TextButton loadButton{ "LOAD FILE" };

    // Core transport sliders.
    Slider volSlider;
    Slider speedSlider;
    Slider posSlider;

    // EQ controls for low/mid/high bands.
    Slider lowEqSlider;
    Slider midEqSlider;
    Slider highEqSlider;

    // Hot cue controls (8 pads + clear all).
    std::array<std::unique_ptr<TextButton>, 8> hotCueButtons;
    TextButton clearHotCuesButton{ "CLEAR CUES" };

    WaveformDisplay waveformDisplay;
    DJAudioPlayer* player{ nullptr };

    // Track-specific state used for persistence callbacks.
    std::array<double, 8> hotCuesSeconds{};
    juce::String currentTrackId;
    bool isInternalPositionUpdate{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckGUI)
};
