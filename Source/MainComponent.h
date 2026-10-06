#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "DJAudioPlayer.h"
#include "DeckGUI.h"
#include <array>
#include <vector>

// Main app container: audio graph, two decks, library table and persistence.
class MainComponent : public AudioAppComponent,
                      public juce::TableListBoxModel,
                      public juce::Button::Listener
{
public:
    MainComponent();
    ~MainComponent();

    // Audio device lifecycle callbacks.
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // Standard JUCE component callbacks.
    void paint(Graphics& g) override;
    void resized() override;

    // Table model callbacks for the music library list.
    int getNumRows() override;
    void paintRowBackground(Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(Graphics& g,
                   int rowNumber,
                   int columnId,
                   int width,
                   int height,
                   bool rowIsSelected) override;

    // Library action button callbacks.
    void buttonClicked(juce::Button* button) override;

private:
    // Persisted track entry for library, BPM and hot cues.
    struct TrackEntry
    {
        juce::String id;
        juce::String name;
        juce::String filePath;
        double durationSeconds{ 0.0 };
        double bpm{ 0.0 };
        std::array<double, 8> hotCues{};
    };

    // Add one or more files into library (with metadata + BPM).
    void addFilesToLibrary(const juce::Array<juce::File>& files);

    // Add one file if missing, or keep existing row if already present.
    void addOrUpdateTrackInLibrary(const juce::File& file);

    // Load selected library row into a chosen deck.
    void loadSelectedTrackToDeck(DeckGUI& deck);

    // Hot cue update callback used by both decks.
    void updateTrackHotCue(const juce::String& trackId, int cueIndex, double cueSeconds);

    // Return index for a track id (path-based id), or -1.
    int findTrackIndexById(const juce::String& trackId) const;

    // Read track duration from the audio file header.
    double readDurationSeconds(const juce::File& file);

    // Estimate BPM using a lightweight onset-envelope autocorrelation.
    double estimateBpm(const juce::File& file);

    // Resolve path for app state JSON file.
    juce::File getStateFile() const;

    // Save and load full library + cue + EQ state.
    void saveStateToDisk() const;
    void loadStateFromDisk();

    // Audio backend shared by both decks.
    AudioFormatManager formatManager;
    AudioThumbnailCache thumbCache{ 100 };

    DJAudioPlayer player1{ formatManager };
    DeckGUI deckGUI1{ &player1, formatManager, thumbCache, "Deck 1" };

    DJAudioPlayer player2{ formatManager };
    DeckGUI deckGUI2{ &player2, formatManager, thumbCache, "Deck 2" };

    MixerAudioSource mixerSource;

    // Library controls and table view.
    juce::Label libraryTitleLabel;
    juce::TextButton addTracksButton{ "Add Tracks" };
    juce::TextButton removeTrackButton{ "Remove Selected" };
    juce::TextButton loadDeck1Button{ "Load -> Deck 1" };
    juce::TextButton loadDeck2Button{ "Load -> Deck 2" };
    juce::TableListBox libraryTable;
    juce::FileChooser libraryFileChooser{ "Select audio files..." };

    std::vector<TrackEntry> libraryTracks;

    // Persisted deck EQ values restored on app startup.
    std::array<float, 3> deck1Eq{ 0.0f, 0.0f, 0.0f };
    std::array<float, 3> deck2Eq{ 0.0f, 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

