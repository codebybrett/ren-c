//
//  File: %sys-logic.h
//  Summary: "LOGIC! Datatype Header"
//  Project: "Rebol 3 Interpreter and Run-time (Ren-C branch)"
//  Homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012 REBOL Technologies
// Copyright 2012-2021 Ren-C Open Source Contributors
// REBOL is a trademark of REBOL Technologies
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// LOGIC! is a simple boolean value type which can be either true or false.
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// * A good source notation for logic literals was never chosen, so #[true]
//   and #[false] have been used.
//

inline static REBVAL *Init_Logic_Core(RELVAL *out, bool flag) {
    RESET_CELL(out, REB_LOGIC, CELL_MASK_NONE);
    PAYLOAD(Logic, out).flag = flag;
  #ifdef ZERO_UNUSED_CELL_FIELDS
    EXTRA(Any, out).trash = nullptr;
  #endif
    return cast(REBVAL*, out);
}

#define Init_Logic(out,flag) \
    Init_Logic_Core(TRACK_CELL_IF_DEBUG(out), (flag))

#define Init_True(out)      Init_Logic((out), true)
#define Init_False(out)     Init_Logic((out), false)

inline static bool VAL_LOGIC(REBCEL(const*) v) {
    assert(CELL_KIND(v) == REB_LOGIC);
    return PAYLOAD(Logic, v).flag;
}


//=//// GLOBALLY AVAILABLE TRUE AND FALSE VALUE CELLS //////////////////////=//

#define FALSE_VALUE \
    c_cast(const REBVAL*, &PG_False_Value)

#define TRUE_VALUE \
    c_cast(const REBVAL*, &PG_True_Value)


//=//// "TRUTHINESS" AND "FALSINESS" ///////////////////////////////////////=//
//
// Like most languages, more things are "truthy" than logic #[true] and more
// things are "falsey" than logic #[false].  NULLs and BLANK!s are also falsey,
// and most values are considered truthy besides BAD-WORD!s, that trigger
// errors when used in conditions.
//
// Despite Rebol's C heritage, the INTEGER! 0 is specifically not "falsey".
//

inline static bool IS_TRUTHY(const RELVAL *v) {
    if (IS_BAD_WORD(v)) {
        //
        // Technically speaking, users need not think of whether the ~null~
        // isotope is "truthy or falsey"...because most functions they would
        // call (like DID) won't see the isotope since they take normal
        // parameters...
        //
        // But internal to the implementation, there is code like:
        //
        //     >> every x [1 2 3] [if x = 2 [null]]
        //     == [1 3]
        //
        // The body result for x = 2 is a ~null~ isotope.  To say whether that
        // isotope is "fundamentally truthy" or that functions like this
        // "make a special exception for null isotopes" is splitting hairs.
        //
        // It seems easier to just tolerate them here vs. asserting it never
        // sees isotopes, and having a separate version of IS_TRUTHY().
        // 
        if (GET_CELL_FLAG(v, ISOTOPE) and VAL_BAD_WORD_ID(v) == SYM_NULL)
            return false;
        fail (Error_Bad_Conditional_Raw());
    }
    if (KIND3Q_BYTE(v) > REB_LOGIC)
        return true;  // includes QUOTED! `if first ['_] [-- "this is truthy"]`
    if (IS_LOGIC(v))
        return VAL_LOGIC(v);
    assert(IS_BLANK(v) or IS_NULLED(v));
    return false;
}

#define IS_FALSEY(v) \
    (not IS_TRUTHY(v))

// Although a BLOCK! value is true, some constructs are safer by not allowing
// literal blocks.  e.g. `if [x] [print "this is not safe"]`.  The evaluated
// bit can let these instances be distinguished.  Note that making *all*
// evaluations safe would be limiting, e.g. `foo: any [false-thing []]`...
// So ANY and ALL use IS_TRUTHY() directly
//
inline static bool IS_CONDITIONAL_TRUE(const REBVAL *v) {
    if (IS_FALSEY(v))
        return false;
    if (KIND3Q_BYTE(v) == REB_BLOCK)
        if (GET_CELL_FLAG(v, UNEVALUATED))
            fail (Error_Block_Conditional_Raw(v));  // !!! Unintended_Literal?
    return true;
}

#define IS_CONDITIONAL_FALSE(v) \
    (not IS_CONDITIONAL_TRUE(v))
