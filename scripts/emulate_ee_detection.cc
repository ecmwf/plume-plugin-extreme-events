#include "nwp_emulator.h"
#include <iostream>

using namespace nwp_utils;

int main(int argc, char** argv) {
    std::cout << "Setting up emulator for extreme event detection..." << std::endl;
    NWPEmulator emulator(argc, argv);
    return emulator.start();
}
