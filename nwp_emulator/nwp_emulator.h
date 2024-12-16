#include "atlas/runtime/AtlasTool.h"

namespace nwp_utils {
class NWPEmulator : public atlas::AtlasTool {

int execute(const Args& args) override;

public:
    NWPEmulator(int argc, char** argv);

private:

    char* plumeConfigPath_;
};
}