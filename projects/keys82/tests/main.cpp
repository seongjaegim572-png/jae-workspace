#include "TestHarness.h"
#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    return jae::test::run ([] { return std::make_unique<KeysProcessor>(); },
                           [] (const juce::String&) { return jae::test::defaultPhrase (48); },
                           4.0, "keys82", argc, argv);
}
