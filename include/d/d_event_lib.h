#ifndef D_EVENT_D_EVENT_LIB_H
#define D_EVENT_D_EVENT_LIB_H

#include <types.h>

// newlib's <ctype.h> on devkitA64 #defines _C (and _U _N _S _P _X _B) as
// integer character-class flags, so the struct member `_C` below expands to
// a numeric constant. push/undef/pop is a no-op on toolchains where _C is
// not a macro (PC).
#pragma push_macro("_C")
#undef _C

class fopAc_ac_c;

template <typename A0>
struct action_class {
    typedef bool (A0::*fptr)();
    fptr init;
    fptr execute;

    action_class(fptr pInit, fptr pExecute) {
        init = pInit;
        execute = pExecute;
    }

    fptr& getInit() { return init; }

    fptr& getExecute() { return execute; }
};

class dEvLib_callback_c {
public:
    dEvLib_callback_c(fopAc_ac_c* param_0) {
        mActor = param_0;
        mAction = NULL;
    }

    bool eventUpdate();
    bool setEvent(int, int, int);
    void orderEvent(int, int, int);
    bool setAction(action_class<dEvLib_callback_c>*);
    bool initAction();
    bool executeAction();
    bool initStart();
    bool executeStart();
    bool initRun();
    bool executeRun();

    virtual ~dEvLib_callback_c() {}
    virtual bool eventStart() { return true; }
    virtual bool eventRun() { return true; }
    virtual bool eventEnd() { return true; }

    /* 0x4 */ fopAc_ac_c* mActor;
    /* 0x8 */ action_class<dEvLib_callback_c>* mAction;
    /* 0xC */ u16 _C;
};

#pragma pop_macro("_C")

#endif /* D_EVENT_D_EVENT_LIB_H */
