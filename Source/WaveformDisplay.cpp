#include "../JuceLibraryCode/JuceHeader.h"
#include "WaveformDisplay.h"

//==============================================================================
WaveformDisplay::WaveformDisplay(AudioFormatManager& formatManagerToUse,
                                 AudioThumbnailCache& cacheToUse)
    : audioThumb(1000, formatManagerToUse, cacheToUse),
      fileLoaded(false),
      position(0.0)
{
    // Listen for thumbnail updates while waveform source is loading.
    audioThumb.addChangeListener(this);
}

WaveformDisplay::~WaveformDisplay()
{
}

void WaveformDisplay::paint(Graphics& g)
{
    // Draw panel background and frame.
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    g.setColour(Colours::grey);
    g.drawRect(getLocalBounds(), 1);

    if (fileLoaded)
    {
        // Draw waveform and current playhead marker.
        g.setColour(Colours::orange);
        audioThumb.drawChannel(g,
                               getLocalBounds(),
                               0,
                               audioThumb.getTotalLength(),
                               0,
                               1.0f);

        g.setColour(Colours::lightgreen);
        g.drawRect(static_cast<int>(position * getWidth()), 0, getWidth() / 70, getHeight());
    }
    else
    {
        // Draw placeholder before any track is loaded.
        g.setColour(Colours::white);
        g.setFont(20.0f);
        g.drawText("File not loaded...", getLocalBounds(), Justification::centred, true);
    }
}

void WaveformDisplay::resized()
{
    // No child components to lay out.
}

void WaveformDisplay::loadURL(URL audioURL)
{
    // Refresh thumbnail source for the selected audio file.
    audioThumb.clear();
    fileLoaded = audioThumb.setSource(new URLInputSource(audioURL));
    repaint();
}

void WaveformDisplay::changeListenerCallback(ChangeBroadcaster*)
{
    // Repaint whenever new waveform thumbnail data arrives.
    repaint();
}

void WaveformDisplay::setPositionRelative(double pos)
{
    // Update playhead cursor only when value changes.
    if (pos != position)
    {
        position = pos;
        repaint();
    }
}
