#include "../sys_prof.h"

idCVar com_callProf("com_callProf", "0", CVAR_BOOL|CVAR_SYSTEM|CVAR_NOCHEAT, "call profiling enabled");
std::vector<ProfMeas> profMeas;

int ScopedTimer::indentation = 0;

void DumpProfMeas()
{
    for (std::vector<ProfMeas>::iterator it = profMeas.begin(); it != profMeas.end(); it++)
    {
        int offset = (16 - it->depth) * 2;
        if (offset < 0)
            offset = 0;
        common->Printf("%s %" PRIu64 " us %s\n", "----------------------------------" + offset, it->time, it->name);
    }
    profMeas.clear();
}
