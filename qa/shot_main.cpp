//==============================================================================
// SHIFT/FX PRO — offscreen UI render (dev tool).
//
// Renders the real editor UI to an image without a display, via
// Component::paintEntireComponent. Not part of the plugin.
//
// NOTE: juce::PNGImageFormat::writeImageToStream was found to emit stale
// pixel data for images produced by paintEntireComponent in this Linux
// environment (verified: in-memory getPixelAt showed correct pixels while
// the PNG did not; the JPEG writer was correct). So this tool writes a
// max-quality JPEG, which is converted to PNG afterwards.
//==============================================================================

#include "../Source/PluginEditor.h"
#include <cstdio>
#include <csignal>
#include <execinfo.h>
#include <unistd.h>

static void segvHandler (int)
{
    void* frames[32];
    const int n = backtrace (frames, 32);
    backtrace_symbols_fd (frames, n, STDERR_FILENO);
    _exit (139);
}

int main()
{
    std::signal (SIGSEGV, segvHandler);
    juce::ScopedJuceInitialiser_GUI gui;

    ShiftFxProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // Demo state so the screenshot looks alive.
    if (auto* p = proc.apvts.getParameter (ShiftFxProcessor::pidOn))
        p->setValueNotifyingHost (1.0f);
    proc.inLevelL.store (0.72f);  proc.inLevelR.store (0.61f);
    proc.outLevelL.store (0.55f); proc.outLevelR.store (0.48f);
    proc.effectiveBpm.store (128.0f);
    proc.tempoFromHost.store (true);

    ShiftFxEditor ed (proc);
    // Do NOT setVisible(true): the editor paints fine hidden, and this keeps
    // the snapshot as the single paint pass.

    // Let the 30 Hz UI timer tick a few times (BPM readout, meters), then
    // stop the dispatch loop.
    struct Stopper : juce::Timer
    {
        void timerCallback() override
        {
            juce::MessageManager::getInstance()->stopDispatchLoop();
            stopTimer();
        }
    } stopper;
    stopper.startTimer (400);
    juce::MessageManager::getInstance()->runDispatchLoop();

    // Manual render into our own image (1.5x for a crisp screenshot).
    juce::Image img (juce::Image::ARGB, 1800, 1050, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (1.5f, 1.5f));
        ed.paintEntireComponent (g, true);
    }
    std::printf ("editor bounds: %d x %d\n", ed.getWidth(), ed.getHeight());

    juce::File f ("/tmp/shiftfx-screenshot.jpg");
    juce::FileOutputStream fos (f);
    if (fos.openedOk())
    {
        juce::JPEGImageFormat jpg;
        jpg.setQuality (1.0f);
        jpg.writeImageToStream (img, fos);
        std::printf ("wrote %s (%d x %d)\n",
                     f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        return 0;
    }

    std::printf ("FAILED to open output file\n");
    return 1;
}
