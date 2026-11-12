#include "TestHarness.h"
#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    return jae::test::run ([] { return std::make_unique<HaloProcessor>(); },
                           [] (const juce::String&) { return jae::test::defaultPhrase (55); },
                           4.0, "halo", argc, argv);
}
