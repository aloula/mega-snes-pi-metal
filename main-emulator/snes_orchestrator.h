#ifndef SNES_ORCHESTRATOR_H
#define SNES_ORCHESTRATOR_H

#include "../shared/lr_snes2010_orchestrator.h"

class CSNESOrchestrator : public CLRSnes2010Orchestrator {
public:
    CSNESOrchestrator(FATFS *pFileSystem)
        : CLRSnes2010Orchestrator(pFileSystem) {}
};

#endif
