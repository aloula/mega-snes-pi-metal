#ifndef EMU_ORCHESTRATOR_H
#define EMU_ORCHESTRATOR_H

#include "../shared/lr_snes2010_orchestrator.h"

class CEmuOrchestrator : public CLRSnes2010Orchestrator {
public:
    CEmuOrchestrator(FATFS *pFileSystem)
        : CLRSnes2010Orchestrator(pFileSystem) {}
};

#endif
