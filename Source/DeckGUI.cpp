#include "../JuceLibraryCode/JuceHeader.h"
#include "DeckGUI.h"

//==============================================================================
DeckGUI::DeckGUI(DJAudioPlayer* _player,
                 AudioFormatManager& formatManagerToUse,
                 AudioThumbnailCache& cacheToUse,
                 const juce::String& deckTitle)
    : waveformDisplay(formatManagerToUse, cacheToUse),
      player(_player)
{
    // Start with no cue markers assigned for this deck.
    hotCuesSeconds.fill(-1.0);

    // Configure static labels for deck name, track name and BPM.
    deckTitleLabel.setText(deckTitle, juce::dontSendNotification);
    deckTitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(deckTitleLabel);

    trackInfoLabel.setText("No track loaded", juce::dontSendNotification);
    trackInfoLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(trackInfoLabel);

    bpmLabel.setText("BPM: --", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bpmLabel);

    // Register transport buttons.
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(loadButton);
    playButton.addListener(this);
    stopButton.addListener(this);
    loadButton.addListener(this);

    // Configure and register core transport sliders.
    volSlider.setRange(0.0, 1.0, 0.01);
    volSlider.setValue(0.8);
    volSlider.setTextValueSuffix(" gain");
    addAndMakeVisible(volSlider);
    volSlider.addListener(this);

    speedSlider.setRange(0.1, 4.0, 0.01);
    speedSlider.setValue(1.0);
    speedSlider.setTextValueSuffix(" x speed");
    addAndMakeVisible(speedSlider);
    speedSlider.addListener(this);

    posSlider.setRange(0.0, 1.0, 0.001);
    posSlider.setValue(0.0);
    addAndMakeVisible(posSlider);
    posSlider.addListener(this);

    // Configure and register low/mid/high EQ sliders in dB.
    lowEqSlider.setRange(-24.0, 24.0, 0.1);
    lowEqSlider.setValue(0.0);
    lowEqSlider.setTextValueSuffix(" dB Low");
    addAndMakeVisible(lowEqSlider);
    lowEqSlider.addListener(this);

    midEqSlider.setRange(-24.0, 24.0, 0.1);
    midEqSlider.setValue(0.0);
    midEqSlider.setTextValueSuffix(" dB Mid");
    addAndMakeVisible(midEqSlider);
    midEqSlider.addListener(this);

    highEqSlider.setRange(-24.0, 24.0, 0.1);
    highEqSlider.setValue(0.0);
    highEqSlider.setTextValueSuffix(" dB High");
    addAndMakeVisible(highEqSlider);
    highEqSlider.addListener(this);

    // Push default transport values into the player state.
    player->setGain(volSlider.getValue());
    player->setSpeed(speedSlider.getValue());
    player->setEQGains(static_cast<float>(lowEqSlider.getValue()),
                       static_cast<float>(midEqSlider.getValue()),
                       static_cast<float>(highEqSlider.getValue()));

    // Build 8 hot cue pads that can set/jump cues (shift-click to overwrite).
    for (int i = 0; i < static_cast<int>(hotCueButtons.size()); ++i)
    {
        hotCueButtons[static_cast<size_t>(i)] = std::make_unique<TextButton>("CUE " + juce::String(i + 1));
        addAndMakeVisible(*hotCueButtons[static_cast<size_t>(i)]);
        hotCueButtons[static_cast<size_t>(i)]->addListener(this);
    }

    addAndMakeVisible(clearHotCuesButton);
    clearHotCuesButton.addListener(this);

    // Draw waveform and update it with transport timer ticks.
    addAndMakeVisible(waveformDisplay);
    updateHotCueButtonTexts();
    startTimer(60);
}

DeckGUI::~DeckGUI()
{
    // Stop timer before this component is destroyed.
    stopTimer();
}

void DeckGUI::paint(Graphics& g)
{
    // Paint a simple deck panel with a border and title section.
    g.fillAll(Colours::black.withAlpha(0.08f));
    g.setColour(Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1);
}

void DeckGUI::resized()
{
    // Layout transport controls, waveform, hot cues and EQ controls.
    const int margin = 8;
    const int rowH = 28;
    const int buttonH = 30;
    const int sliderH = 34;
    const int cueH = 30;
    int y = margin;

    deckTitleLabel.setBounds(margin, y, getWidth() / 2, rowH);
    bpmLabel.setBounds(getWidth() / 2, y, getWidth() / 2 - margin, rowH);
    y += rowH;

    trackInfoLabel.setBounds(margin, y, getWidth() - (2 * margin), rowH);
    y += rowH + 4;

    const int buttonW = (getWidth() - (margin * 4)) / 3;
    playButton.setBounds(margin, y, buttonW, buttonH);
    stopButton.setBounds(margin + buttonW + margin, y, buttonW, buttonH);
    loadButton.setBounds(margin + (buttonW + margin) * 2, y, buttonW, buttonH);
    y += buttonH + 4;

    volSlider.setBounds(margin, y, getWidth() - (2 * margin), sliderH);
    y += sliderH;
    speedSlider.setBounds(margin, y, getWidth() - (2 * margin), sliderH);
    y += sliderH;
    posSlider.setBounds(margin, y, getWidth() - (2 * margin), sliderH);
    y += sliderH + 6;

    const int waveformH = juce::jmax(90, getHeight() / 6);
    waveformDisplay.setBounds(margin, y, getWidth() - (2 * margin), waveformH);
    y += waveformH + 6;

    const int cueW = (getWidth() - (margin * 5)) / 4;
    for (int i = 0; i < 8; ++i)
    {
        const int row = i / 4;
        const int col = i % 4;
        hotCueButtons[static_cast<size_t>(i)]->setBounds(
            margin + col * (cueW + margin),
            y + row * (cueH + 4),
            cueW,
            cueH);
    }
    y += (cueH * 2) + 8;

    clearHotCuesButton.setBounds(margin, y, getWidth() - (2 * margin), buttonH);
    y += buttonH + 4;

    lowEqSlider.setBounds(margin, y, getWidth() - (2 * margin), sliderH);
    y += sliderH;
    midEqSlider.setBounds(margin, y, getWidth() - (2 * margin), sliderH);
    y += sliderH;
    highEqSlider.setBounds(margin, y, getWidth() - (2 * margin), sliderH);
}

void DeckGUI::buttonClicked(Button* button)
{
    // Handle playback transport actions.
    if (button == &playButton)
    {
        player->start();
        return;
    }

    if (button == &stopButton)
    {
        player->stop();
        return;
    }

    // Load file directly from this deck and notify MainComponent.
    if (button == &loadButton)
    {
        auto chooserFlags = FileBrowserComponent::canSelectFiles;
        fChooser.launchAsync(chooserFlags, [this](const FileChooser& chooser)
        {
            const auto chosenFile = chooser.getResult();
            if (chosenFile.existsAsFile())
            {
                applyTrackLoad(chosenFile,
                               chosenFile.getFullPathName(),
                               chosenFile.getFileNameWithoutExtension(),
                               0.0,
                               nullptr);

                // Reference: MainComponent::addOrUpdateTrackInLibrary persists this track.
                if (onTrackLoaded)
                {
                    onTrackLoaded(chosenFile);
                }
            }
        });
        return;
    }

    // Clear all cues for current track and propagate persistence updates.
    if (button == &clearHotCuesButton)
    {
        if (currentTrackId.isNotEmpty())
        {
            for (int i = 0; i < static_cast<int>(hotCuesSeconds.size()); ++i)
            {
                hotCuesSeconds[static_cast<size_t>(i)] = -1.0;
                if (onHotCueChanged)
                {
                    onHotCueChanged(currentTrackId, i, -1.0);
                }
            }
        }

        updateHotCueButtonTexts();
        return;
    }

    // Hot cue buttons: click to jump/set, shift-click to overwrite.
    for (int i = 0; i < static_cast<int>(hotCueButtons.size()); ++i)
    {
        if (button == hotCueButtons[static_cast<size_t>(i)].get())
        {
            if (currentTrackId.isEmpty())
            {
                return;
            }

            const bool shiftDown = ModifierKeys::getCurrentModifiers().isShiftDown();
            const bool hasCue = hotCuesSeconds[static_cast<size_t>(i)] >= 0.0;
            if (shiftDown || !hasCue)
            {
                assignHotCue(i);
            }
            else
            {
                triggerHotCue(i);
            }

            return;
        }
    }
}

void DeckGUI::sliderValueChanged(Slider* slider)
{
    // Apply main transport controls to player.
    if (slider == &volSlider)
    {
        player->setGain(slider->getValue());
        return;
    }

    if (slider == &speedSlider)
    {
        player->setSpeed(slider->getValue());
        return;
    }

    if (slider == &posSlider)
    {
        if (!isInternalPositionUpdate)
        {
            player->setPositionRelative(slider->getValue());
        }

        return;
    }

    // Update deck EQ and notify MainComponent for persistence.
    if (slider == &lowEqSlider || slider == &midEqSlider || slider == &highEqSlider)
    {
        const auto lowDb = static_cast<float>(lowEqSlider.getValue());
        const auto midDb = static_cast<float>(midEqSlider.getValue());
        const auto highDb = static_cast<float>(highEqSlider.getValue());

        player->setEQGains(lowDb, midDb, highDb);
        if (onEqChanged)
        {
            // Reference: MainComponent::saveStateToDisk stores these values.
            onEqChanged(lowDb, midDb, highDb);
        }

        return;
    }
}

bool DeckGUI::isInterestedInFileDrag(const StringArray& files)
{
    // Accept drag and drop if at least one file is present.
    return files.size() > 0;
}

void DeckGUI::filesDropped(const StringArray& files, int, int)
{
    // Load first dropped file and notify library state manager.
    if (files.size() > 0)
    {
        const juce::File droppedFile(files[0]);
        if (droppedFile.existsAsFile())
        {
            applyTrackLoad(droppedFile,
                           droppedFile.getFullPathName(),
                           droppedFile.getFileNameWithoutExtension(),
                           0.0,
                           nullptr);

            if (onTrackLoaded)
            {
                onTrackLoaded(droppedFile);
            }
        }
    }
}

void DeckGUI::timerCallback()
{
    // Keep position slider and waveform cursor in sync with playback.
    // Skip the slider while the user is dragging it so the two don't fight.
    const auto relativePos = player->getPositionRelative();
    if (!posSlider.isMouseButtonDown())
    {
        isInternalPositionUpdate = true;
        posSlider.setValue(relativePos, juce::dontSendNotification);
        isInternalPositionUpdate = false;
    }

    waveformDisplay.setPositionRelative(relativePos);
}

void DeckGUI::loadTrack(const juce::File& audioFile,
                        const juce::String& trackId,
                        const juce::String& trackName,
                        double bpm,
                        const std::array<double, 8>& cues)
{
    // Load selected library entry into this deck with restored metadata.
    applyTrackLoad(audioFile, trackId, trackName, bpm, &cues);
}

void DeckGUI::setEqValues(float lowDb, float midDb, float highDb)
{
    // Restore EQ UI values without recursive callbacks.
    lowEqSlider.setValue(lowDb, juce::dontSendNotification);
    midEqSlider.setValue(midDb, juce::dontSendNotification);
    highEqSlider.setValue(highDb, juce::dontSendNotification);
    player->setEQGains(lowDb, midDb, highDb);
}

void DeckGUI::applyTrackLoad(const juce::File& audioFile,
                             const juce::String& newTrackId,
                             const juce::String& trackName,
                             double bpm,
                             const std::array<double, 8>* cuesToApply)
{
    // Load selected audio and reset playhead indicators.
    player->loadURL(URL{ audioFile });
    waveformDisplay.loadURL(URL{ audioFile });
    posSlider.setValue(0.0, juce::dontSendNotification);

    // Track id ties this deck state back to library persistence.
    currentTrackId = (newTrackId.isNotEmpty() ? newTrackId : audioFile.getFullPathName());

    // Restore cues if provided from the library; otherwise clear them.
    if (cuesToApply != nullptr)
    {
        hotCuesSeconds = *cuesToApply;
    }
    else
    {
        hotCuesSeconds.fill(-1.0);
    }

    // Update status labels for user context.
    trackInfoLabel.setText(trackName, juce::dontSendNotification);
    if (bpm > 0.0)
    {
        bpmLabel.setText("BPM: " + juce::String(bpm, 1), juce::dontSendNotification);
    }
    else
    {
        bpmLabel.setText("BPM: --", juce::dontSendNotification);
    }

    updateHotCueButtonTexts();
}

void DeckGUI::updateHotCueButtonTexts()
{
    // Show compact cue status on each cue pad.
    for (int i = 0; i < static_cast<int>(hotCueButtons.size()); ++i)
    {
        const auto cueSeconds = hotCuesSeconds[static_cast<size_t>(i)];
        if (cueSeconds >= 0.0)
        {
            hotCueButtons[static_cast<size_t>(i)]->setButtonText(
                "C" + juce::String(i + 1) + " " + juce::String(cueSeconds, 1) + "s");
        }
        else
        {
            hotCueButtons[static_cast<size_t>(i)]->setButtonText("C" + juce::String(i + 1) + " SET");
        }
    }
}

void DeckGUI::assignHotCue(int cueIndex)
{
    // Store current playhead in selected cue slot and persist immediately.
    const auto positionSecs = player->getCurrentPositionInSeconds();
    hotCuesSeconds[static_cast<size_t>(cueIndex)] = positionSecs;
    updateHotCueButtonTexts();

    if (onHotCueChanged)
    {
        onHotCueChanged(currentTrackId, cueIndex, positionSecs);
    }
}

void DeckGUI::triggerHotCue(int cueIndex)
{
    // Jump deck playhead to saved cue location.
    const auto cueSeconds = hotCuesSeconds[static_cast<size_t>(cueIndex)];
    if (cueSeconds >= 0.0)
    {
        player->setPosition(cueSeconds);
    }
}

