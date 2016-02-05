#ifndef ID_SYS_PROF_H__
#define ID_SYS_PROF_H__
#include <inttypes.h>
#include "../idlib/precompiled.h"
#include <vector>

extern idCVar com_callProf;

uint64_t Sys_Microseconds();

struct ProfMeas
{
    const char *name;
    int depth;
    uint64_t time;
    ProfMeas(const char *name, int depth, uint64_t time) : name(name), depth(depth), time(time) { }
};
extern std::vector<ProfMeas> profMeas;

void DumpProfMeas();

class ScopedTimer
{
public:
    ScopedTimer(const char *name) : name(name), time(0)
    {
        indentation++;
        if (com_callProf.GetBool())
        {
            time = Sys_Microseconds();
        }
    }

    ~ScopedTimer()
    {
        indentation--;
        if (com_callProf.GetBool())
        {
            profMeas.push_back(ProfMeas(name, indentation, Sys_Microseconds() - time));
        }
    }
private:
    static int indentation;
    const char *name;
    uint64_t time;
};

#define SCOPED_TIMER_CC2(x, y) x##y
#define SCOPED_TIMER_CC(x, y) SCOPED_TIMER_CC2(x, y)
#define SCOPED_TIMER(name) ScopedTimer SCOPED_TIMER_CC(timer_, __LINE__)(name)

#endif
