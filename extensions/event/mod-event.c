//
//  File: %mod-event.c
//  Summary: "EVENT! extension main C file"
//  Section: Extension
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologiesg
// Copyright 2012-2019 Rebol Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
//=////////////////////////////////////////////////////////////////////////=//
//
// See notes in %extensions/event/README.md
//

#include "sys-core.h"

#include "tmp-mod-event.h"

#include "reb-event.h"

//
//  register-event-hooks: native [
//
//  {Make the EVENT! datatype work with GENERIC actions, comparison ops, etc}
//
//      return: [void!]
//  ]
//
REBNATIVE(register_event_hooks)
{
    EVENT_INCLUDE_PARAMS_OF_REGISTER_EVENT_HOOKS;

    // !!! See notes on Hook_Datatype for this poor-man's substitute for a
    // coherent design of an extensible object system (as per Lisp's CLOS)
    //
    Hook_Datatype(
        REB_EVENT,
        &T_Event,
        &PD_Event,
        &CT_Event,
        &MAKE_Event,
        &TO_Event,
        &MF_Event
    );

    Startup_Event_Scheme();

    return Init_Void(D_OUT);
}


//
//  unregister-event-hooks: native [
//
//  {Remove behaviors for EVENT! added by REGISTER-EVENT-HOOKS}
//
//      return: [void!]
//  ]
//
REBNATIVE(unregister_event_hooks)
{
    EVENT_INCLUDE_PARAMS_OF_UNREGISTER_EVENT_HOOKS;

    Shutdown_Event_Scheme();

    Unhook_Datatype(REB_EVENT);

    return Init_Void(D_OUT);
}


//
//  get-event-actor-handle: native [
//
//  {Retrieve handle to the native actor for events (system, event, callback)}
//
//      return: [handle!]
//  ]
//
REBNATIVE(get_event_actor_handle)
{
    Make_Port_Actor_Handle(D_OUT, &Event_Actor);
    return D_OUT;
}


//
//  map-event: native [
//
//  {Returns event with inner-most graphical object and coordinate.}
//
//      event [event!]
//  ]
//
REBNATIVE(map_event)
{
    EVENT_INCLUDE_PARAMS_OF_MAP_EVENT;

    REBVAL *e = ARG(event);

    if (VAL_EVENT_MODEL(e) != EVM_GUI)
        fail ("Can't use MAP-EVENT on non-GUI event");

    REBGOB *g = cast(REBGOB*, VAL_EVENT_NODE(e));
    if (not g)
        RETURN (e);  // !!! Should this have been an error?

    if (not (VAL_EVENT_FLAGS(e) & EVF_HAS_XY))
        RETURN (e);  // !!! Should this have been an error?

    REBD32 x = VAL_EVENT_X(e);
    REBD32 y = VAL_EVENT_Y(e);

    DECLARE_LOCAL (gob);
    Init_Gob(gob, g);  // !!! Efficiency hack: %reb-event.h has Init_Gob()
    PUSH_GC_GUARD(gob);

    REBVAL *mapped = rebValue(
        "map-gob-offset", gob, "make pair! [", rebI(x), rebI(y), "]",
    rebEND);

    // For efficiency, %reb-event.h is able to store direct REBGOB pointers
    // (This loses any index information or other cell-instance properties)
    //
    assert(VAL_EVENT_MODEL(e) == EVM_GUI);  // should still be true
    SET_VAL_EVENT_NODE(e, VAL_GOB(mapped));

    rebRelease(mapped);

    assert(VAL_EVENT_FLAGS(e) & EVF_HAS_XY);  // should still be true
    SET_VAL_EVENT_X(e, ROUND_TO_INT(x));
    SET_VAL_EVENT_Y(e, ROUND_TO_INT(y));

    RETURN (e);
}
