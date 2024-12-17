#include "ee_plugin.h"
 
namespace ExtremeEventPlugin {
 
static plume::PluginCoreBuilder<EEPluginCore> EEPluginCoreBuilder;
 
REGISTER_LIBRARY(EEPlugin);
 
}