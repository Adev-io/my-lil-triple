// test.cpp: checks the core with plain asserts. Run: make test
// Author: Ari Klopfer / A_Development. MIT
#include "check.h"
#include "test_adsr.h"
#include "test_chords.h"
#include "test_roll.h"
#include "test_osc.h"
#include "test_svf.h"
#include "test_tools.h"
#include "test_voice.h"

int main()
{
    testAdsr();
    testChords();
    testRollData();
    testRollClock();
    testRollPlayer();
    testToolsQuick();
    testToolsShape();
    testToolsTime();
    testOsc();
    testSvf();
    testVoice();
    std::printf ("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
