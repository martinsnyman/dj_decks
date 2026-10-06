#include "MainComponent.h"
#include <cmath>
#include <limits>

//==============================================================================
MainComponent::MainComponent()
{
    // Set a wider default size to comfortably show 2 decks + central library.
    setSize(1500, 760);

    // Connect both decks to the mixer once; the mixer prepares/releases its inputs.
    mixerSource.addInputSource(&player1, false);
    mixerSource.addInputSource(&player2, false);

    // Request audio output channels for realtime playback.
    if (RuntimePermissions::isRequired(RuntimePermissions::recordAudio)
        && !RuntimePermissions::isGranted(RuntimePermissions::recordAudio))
    {
        RuntimePermissions::request(RuntimePermissions::recordAudio,
                                    [&](bool granted)
                                    {
                                        if (granted)
                                        {
                                            setAudioChannels(0, 2);
                                        }
                                    });
    }
    else
    {
        setAudioChannels(0, 2);
    }

    // Register built-in audio formats before metadata loading/BPM analysis.
    formatManager.registerBasicFormats();

    // Add deck components.
    addAndMakeVisible(deckGUI1);
    addAndMakeVisible(deckGUI2);

    // Configure central library title and action buttons.
    libraryTitleLabel.setText("Music Library", juce::dontSendNotification);
    libraryTitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(libraryTitleLabel);

    addAndMakeVisible(addTracksButton);
    addAndMakeVisible(removeTrackButton);
    addAndMakeVisible(loadDeck1Button);
    addAndMakeVisible(loadDeck2Button);

    addTracksButton.addListener(this);
    removeTrackButton.addListener(this);
    loadDeck1Button.addListener(this);
    loadDeck2Button.addListener(this);

    // Configure table columns for key track metadata.
    libraryTable.setModel(this);
    libraryTable.getHeader().addColumn("Track", 1, 180);
    libraryTable.getHeader().addColumn("Duration", 2, 80);
    libraryTable.getHeader().addColumn("BPM", 3, 70);
    libraryTable.getHeader().addColumn("Source", 4, 260);
    libraryTable.setMultipleSelectionEnabled(false);
    addAndMakeVisible(libraryTable);

    // Load previously persisted library/cue/EQ state.
    loadStateFromDisk();

    // Forward direct deck file loads into persisted library state.
    deckGUI1.onTrackLoaded = [this](const juce::File& file)
    {
        addOrUpdateTrackInLibrary(file);
        const auto trackId = file.getFullPathName();
        const int index = findTrackIndexById(trackId);
        if (index >= 0)
        {
            const auto& track = libraryTracks[static_cast<size_t>(index)];
            deckGUI1.loadTrack(file, track.id, track.name, track.bpm, track.hotCues);
        }

        libraryTable.updateContent();
        libraryTable.repaint();
        saveStateToDisk();
    };

    deckGUI2.onTrackLoaded = [this](const juce::File& file)
    {
        addOrUpdateTrackInLibrary(file);
        const auto trackId = file.getFullPathName();
        const int index = findTrackIndexById(trackId);
        if (index >= 0)
        {
            const auto& track = libraryTracks[static_cast<size_t>(index)];
            deckGUI2.loadTrack(file, track.id, track.name, track.bpm, track.hotCues);
        }

        libraryTable.updateContent();
        libraryTable.repaint();
        saveStateToDisk();
    };

    // Persist hot cue changes from either deck.
    deckGUI1.onHotCueChanged = [this](const juce::String& trackId, int cueIndex, double cueSeconds)
    {
        updateTrackHotCue(trackId, cueIndex, cueSeconds);
    };

    deckGUI2.onHotCueChanged = [this](const juce::String& trackId, int cueIndex, double cueSeconds)
    {
        updateTrackHotCue(trackId, cueIndex, cueSeconds);
    };

    // Persist deck EQ changes whenever user touches deck EQ controls.
    deckGUI1.onEqChanged = [this](float lowDb, float midDb, float highDb)
    {
        deck1Eq = { lowDb, midDb, highDb };
        saveStateToDisk();
    };

    deckGUI2.onEqChanged = [this](float lowDb, float midDb, float highDb)
    {
        deck2Eq = { lowDb, midDb, highDb };
        saveStateToDisk();
    };

    // Restore persisted deck EQ values into UI + audio engine.
    deckGUI1.setEqValues(deck1Eq[0], deck1Eq[1], deck1Eq[2]);
    deckGUI2.setEqValues(deck2Eq[0], deck2Eq[1], deck2Eq[2]);

    // Select first row by default if library has entries.
    if (!libraryTracks.empty())
    {
        libraryTable.selectRow(0);
    }
}

MainComponent::~MainComponent()
{
    // Persist latest state before app shutdown.
    saveStateToDisk();

    // Shut down audio device and clear audio source graph.
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    // Mixer prepares both players (inputs were added in the constructor).
    // Adding inputs here would duplicate them whenever the audio device restarts.
    mixerSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill)
{
    // Render mixed audio from both decks into device output.
    mixerSource.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    // Mixer releases both players as well.
    mixerSource.releaseResources();
}

void MainComponent::paint(Graphics& g)
{
    // Draw a neutral app background.
    g.fillAll(Colours::darkslategrey.darker(0.7f));
}

void MainComponent::resized()
{
    // Split main UI into: deck1 | library | deck2.
    const int margin = 8;
    const int middleW = getWidth() / 3;
    const int sideW = (getWidth() - middleW) / 2;

    deckGUI1.setBounds(0, 0, sideW, getHeight());
    deckGUI2.setBounds(sideW + middleW, 0, getWidth() - (sideW + middleW), getHeight());

    juce::Rectangle<int> middleArea(sideW + margin, margin, middleW - (2 * margin), getHeight() - (2 * margin));

    libraryTitleLabel.setBounds(middleArea.removeFromTop(28));
    middleArea.removeFromTop(4);

    auto addRemoveRow = middleArea.removeFromTop(30);
    addTracksButton.setBounds(addRemoveRow.removeFromLeft(addRemoveRow.getWidth() / 2 - 2));
    addRemoveRow.removeFromLeft(4);
    removeTrackButton.setBounds(addRemoveRow);

    middleArea.removeFromTop(4);

    auto loadRow = middleArea.removeFromTop(30);
    loadDeck1Button.setBounds(loadRow.removeFromLeft(loadRow.getWidth() / 2 - 2));
    loadRow.removeFromLeft(4);
    loadDeck2Button.setBounds(loadRow);

    middleArea.removeFromTop(6);
    libraryTable.setBounds(middleArea);
}

int MainComponent::getNumRows()
{
    // Table row count equals number of library entries.
    return static_cast<int>(libraryTracks.size());
}

void MainComponent::paintRowBackground(Graphics& g,
                                       int rowNumber,
                                       int,
                                       int,
                                       bool rowIsSelected)
{
    // Highlight selected row, stripe all others.
    if (rowIsSelected)
    {
        g.fillAll(Colours::cornflowerblue.withAlpha(0.45f));
    }
    else if (rowNumber % 2 == 0)
    {
        g.fillAll(Colours::black.withAlpha(0.12f));
    }
    else
    {
        g.fillAll(Colours::black.withAlpha(0.2f));
    }
}

void MainComponent::paintCell(Graphics& g,
                              int rowNumber,
                              int columnId,
                              int width,
                              int height,
                              bool)
{
    // Paint each table cell with the corresponding track metadata.
    if (rowNumber < 0 || rowNumber >= getNumRows())
    {
        return;
    }

    const auto& track = libraryTracks[static_cast<size_t>(rowNumber)];
    juce::String text;

    if (columnId == 1)
    {
        text = track.name;
    }
    else if (columnId == 2)
    {
        const int totalSec = static_cast<int>(std::round(track.durationSeconds));
        const int minutes = totalSec / 60;
        const int seconds = totalSec % 60;
        text = juce::String::formatted("%d:%02d", minutes, seconds);
    }
    else if (columnId == 3)
    {
        text = (track.bpm > 0.0 ? juce::String(track.bpm, 1) : "--");
    }
    else if (columnId == 4)
    {
        text = juce::File(track.filePath).getFileName();
    }

    g.setColour(Colours::white);
    g.drawText(text, 6, 0, width - 8, height, Justification::centredLeft, true);

    g.setColour(Colours::black.withAlpha(0.35f));
    g.fillRect(width - 1, 0, 1, height);
}

void MainComponent::buttonClicked(juce::Button* button)
{
    // Add one or more files into the persistent music library.
    if (button == &addTracksButton)
    {
        libraryFileChooser.launchAsync(juce::FileBrowserComponent::canSelectFiles
                                           | juce::FileBrowserComponent::canSelectMultipleItems,
                                       [this](const juce::FileChooser& fc)
                                       {
                                           addFilesToLibrary(fc.getResults());
                                       });
        return;
    }

    // Remove currently selected track from library state.
    if (button == &removeTrackButton)
    {
        const int selectedRow = libraryTable.getSelectedRow();
        if (selectedRow >= 0 && selectedRow < getNumRows())
        {
            libraryTracks.erase(libraryTracks.begin() + selectedRow);
            libraryTable.updateContent();
            libraryTable.repaint();
            saveStateToDisk();
        }
        return;
    }

    // Load selected library track into deck 1.
    if (button == &loadDeck1Button)
    {
        loadSelectedTrackToDeck(deckGUI1);
        return;
    }

    // Load selected library track into deck 2.
    if (button == &loadDeck2Button)
    {
        loadSelectedTrackToDeck(deckGUI2);
        return;
    }
}

void MainComponent::addFilesToLibrary(const juce::Array<juce::File>& files)
{
    // Add all chosen files and refresh table once at the end.
    for (const auto& file : files)
    {
        addOrUpdateTrackInLibrary(file);
    }

    libraryTable.updateContent();
    libraryTable.repaint();
    saveStateToDisk();
}

void MainComponent::addOrUpdateTrackInLibrary(const juce::File& file)
{
    if (!file.existsAsFile())
    {
        return;
    }

    // Use absolute path as stable track id for cue persistence.
    const auto trackId = file.getFullPathName();
    if (findTrackIndexById(trackId) >= 0)
    {
        return;
    }

    TrackEntry entry;
    entry.id = trackId;
    entry.name = file.getFileNameWithoutExtension();
    entry.filePath = file.getFullPathName();
    entry.durationSeconds = readDurationSeconds(file);
    entry.bpm = estimateBpm(file);
    entry.hotCues.fill(-1.0);

    libraryTracks.push_back(entry);
}

void MainComponent::loadSelectedTrackToDeck(DeckGUI& deck)
{
    // Push selected library entry (including cues + BPM) into target deck.
    const int selectedRow = libraryTable.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= getNumRows())
    {
        return;
    }

    const auto& track = libraryTracks[static_cast<size_t>(selectedRow)];
    const juce::File audioFile(track.filePath);
    if (!audioFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Track Missing",
            "The selected audio file no longer exists on disk.");
        return;
    }

    deck.loadTrack(audioFile, track.id, track.name, track.bpm, track.hotCues);
}

void MainComponent::updateTrackHotCue(const juce::String& trackId, int cueIndex, double cueSeconds)
{
    // Track deck cue edits and persist instantly to disk.
    const int trackIndex = findTrackIndexById(trackId);
    if (trackIndex < 0)
    {
        return;
    }

    if (cueIndex < 0 || cueIndex >= 8)
    {
        return;
    }

    libraryTracks[static_cast<size_t>(trackIndex)].hotCues[static_cast<size_t>(cueIndex)] = cueSeconds;

    // Reference: DeckGUI::assignHotCue and DeckGUI::buttonClicked(clear) call here.
    saveStateToDisk();
}

int MainComponent::findTrackIndexById(const juce::String& trackId) const
{
    // Linear lookup is sufficient for the small expected library size.
    for (size_t i = 0; i < libraryTracks.size(); ++i)
    {
        if (libraryTracks[i].id == trackId)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

double MainComponent::readDurationSeconds(const juce::File& file)
{
    // Read header metadata to compute track duration.
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader)
    {
        return 0.0;
    }

    if (reader->sampleRate <= 0.0)
    {
        return 0.0;
    }

    return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
}

double MainComponent::estimateBpm(const juce::File& file)
{
    // Lightweight BPM estimation: envelope extraction + autocorrelation.
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader || reader->sampleRate <= 0.0)
    {
        return 0.0;
    }

    constexpr double maxSecondsToScan = 60.0;
    const auto maxSamples = static_cast<int64>(reader->sampleRate * maxSecondsToScan);
    const auto totalSamples = juce::jmin<int64>(reader->lengthInSamples, maxSamples);
    if (totalSamples <= 0)
    {
        return 0.0;
    }

    constexpr int targetEnvelopeRate = 200;
    const int samplesPerEnvelopePoint = juce::jmax(1, static_cast<int>(reader->sampleRate / targetEnvelopeRate));

    juce::AudioBuffer<float> blockBuffer(static_cast<int>(reader->numChannels), 4096);
    std::vector<double> envelope;
    envelope.reserve(static_cast<size_t>(totalSamples / samplesPerEnvelopePoint) + 8);

    int64 readPosition = 0;
    double windowSum = 0.0;
    int windowCount = 0;

    while (readPosition < totalSamples)
    {
        const int toRead = static_cast<int>(juce::jmin<int64>(blockBuffer.getNumSamples(), totalSamples - readPosition));
        if (!reader->read(&blockBuffer, 0, toRead, readPosition, true, true))
        {
            break;
        }

        for (int sample = 0; sample < toRead; ++sample)
        {
            double mono = 0.0;
            for (int channel = 0; channel < blockBuffer.getNumChannels(); ++channel)
            {
                mono += std::abs(blockBuffer.getSample(channel, sample));
            }

            mono /= juce::jmax(1, blockBuffer.getNumChannels());
            windowSum += mono;
            ++windowCount;

            if (windowCount >= samplesPerEnvelopePoint)
            {
                envelope.push_back(windowSum / static_cast<double>(windowCount));
                windowSum = 0.0;
                windowCount = 0;
            }
        }

        readPosition += toRead;
    }

    if (windowCount > 0)
    {
        envelope.push_back(windowSum / static_cast<double>(windowCount));
    }

    if (envelope.size() < 256)
    {
        return 0.0;
    }

    // High-pass envelope by subtracting a moving average to accent beats.
    std::vector<double> highPassed(envelope.size(), 0.0);
    const int maWindow = juce::jmax(8, targetEnvelopeRate / 4);
    double runningSum = 0.0;

    for (size_t i = 0; i < envelope.size(); ++i)
    {
        runningSum += envelope[i];
        if (i >= static_cast<size_t>(maWindow))
        {
            runningSum -= envelope[i - static_cast<size_t>(maWindow)];
        }

        const int currentWindow = static_cast<int>(juce::jmin<size_t>(i + 1, static_cast<size_t>(maWindow)));
        const double movingAverage = runningSum / static_cast<double>(juce::jmax(1, currentWindow));
        highPassed[i] = envelope[i] - movingAverage;
    }

    // Find BPM candidate with strongest autocorrelation in DJ range [70..180].
    double bestScore = -std::numeric_limits<double>::infinity();
    int bestBpm = 0;

    for (int bpm = 70; bpm <= 180; ++bpm)
    {
        const int lag = static_cast<int>(std::round((targetEnvelopeRate * 60.0) / static_cast<double>(bpm)));
        if (lag <= 0 || static_cast<size_t>(lag) >= highPassed.size() / 2)
        {
            continue;
        }

        double score = 0.0;
        for (size_t i = static_cast<size_t>(lag); i < highPassed.size(); ++i)
        {
            score += highPassed[i] * highPassed[i - static_cast<size_t>(lag)];
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestBpm = bpm;
        }
    }

    return (bestBpm > 0 ? static_cast<double>(bestBpm) : 0.0);
}

juce::File MainComponent::getStateFile() const
{
    // Store app state in user app-data directory for cross-run persistence.
    const auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                .getChildFile("OtoDecks");
    return appDataDir.getChildFile("otodecks_state.json");
}

void MainComponent::saveStateToDisk() const
{
    // Serialize library entries, hot cues and deck EQ values into JSON.
    auto root = std::make_unique<juce::DynamicObject>();

    juce::Array<juce::var> trackArray;
    for (const auto& track : libraryTracks)
    {
        auto trackObj = std::make_unique<juce::DynamicObject>();
        trackObj->setProperty("id", track.id);
        trackObj->setProperty("name", track.name);
        trackObj->setProperty("filePath", track.filePath);
        trackObj->setProperty("durationSeconds", track.durationSeconds);
        trackObj->setProperty("bpm", track.bpm);

        juce::Array<juce::var> cueArray;
        for (const auto cue : track.hotCues)
        {
            cueArray.add(cue);
        }
        trackObj->setProperty("hotCues", juce::var(cueArray));

        trackArray.add(juce::var(trackObj.release()));
    }

    root->setProperty("tracks", juce::var(trackArray));

    juce::Array<juce::var> deck1EqArray;
    deck1EqArray.add(deck1Eq[0]);
    deck1EqArray.add(deck1Eq[1]);
    deck1EqArray.add(deck1Eq[2]);
    root->setProperty("deck1Eq", juce::var(deck1EqArray));

    juce::Array<juce::var> deck2EqArray;
    deck2EqArray.add(deck2Eq[0]);
    deck2EqArray.add(deck2Eq[1]);
    deck2EqArray.add(deck2Eq[2]);
    root->setProperty("deck2Eq", juce::var(deck2EqArray));

    const juce::File stateFile = getStateFile();
    stateFile.getParentDirectory().createDirectory();

    // Reference: loadStateFromDisk reads the same JSON schema at startup.
    stateFile.replaceWithText(juce::JSON::toString(juce::var(root.release()), true));
}

void MainComponent::loadStateFromDisk()
{
    // Read persisted JSON state if it exists.
    const juce::File stateFile = getStateFile();
    if (!stateFile.existsAsFile())
    {
        return;
    }

    const auto jsonText = stateFile.loadFileAsString();
    if (jsonText.isEmpty())
    {
        return;
    }

    const juce::var parsed = juce::JSON::parse(jsonText);
    auto* rootObj = parsed.getDynamicObject();
    if (rootObj == nullptr)
    {
        return;
    }

    // Restore track library rows and their persisted cue slots.
    libraryTracks.clear();
    if (auto* tracks = rootObj->getProperty("tracks").getArray())
    {
        for (const auto& trackVar : *tracks)
        {
            auto* trackObj = trackVar.getDynamicObject();
            if (trackObj == nullptr)
            {
                continue;
            }

            TrackEntry entry;
            entry.id = trackObj->getProperty("id").toString();
            entry.name = trackObj->getProperty("name").toString();
            entry.filePath = trackObj->getProperty("filePath").toString();
            entry.durationSeconds = static_cast<double>(trackObj->getProperty("durationSeconds"));
            entry.bpm = static_cast<double>(trackObj->getProperty("bpm"));
            entry.hotCues.fill(-1.0);

            if (auto* cueArray = trackObj->getProperty("hotCues").getArray())
            {
                for (int i = 0; i < juce::jmin(8, cueArray->size()); ++i)
                {
                    entry.hotCues[static_cast<size_t>(i)] = static_cast<double>(cueArray->getReference(i));
                }
            }

            if (entry.id.isEmpty())
            {
                entry.id = entry.filePath;
            }

            if (entry.name.isEmpty())
            {
                entry.name = juce::File(entry.filePath).getFileNameWithoutExtension();
            }

            if (!entry.filePath.isEmpty())
            {
                libraryTracks.push_back(entry);
            }
        }
    }

    // Restore persisted deck EQ values.
    auto readEqArray = [rootObj](const juce::Identifier& key, std::array<float, 3>& target)
    {
        if (auto* eqArray = rootObj->getProperty(key).getArray())
        {
            for (int i = 0; i < juce::jmin(3, eqArray->size()); ++i)
            {
                target[static_cast<size_t>(i)] = static_cast<float>(static_cast<double>(eqArray->getReference(i)));
            }
        }
    };

    readEqArray("deck1Eq", deck1Eq);
    readEqArray("deck2Eq", deck2Eq);

    libraryTable.updateContent();
    libraryTable.repaint();
}




