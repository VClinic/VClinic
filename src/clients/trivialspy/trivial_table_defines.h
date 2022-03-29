#ifndef __TRIVIAL_TABLE_DEFINES_H__
#define __TRIVIAL_TABLE_DEFINES_H__

// Conditions
#define CONDITION_LIST_EMPTY 0
#define INIT_CONDITION_LIST(cvl) cvl=0
#define __C2CMASK(cval) (1<<cval)
#define APPEND_CONDITION(cvl, cval) (cvl = (cvl | __C2CMASK(cval)))
#define HAS_CONDITION(cvl, cval) (cvl & __C2CMASK(cval))
#define CONDITION_LIST_IS_EMPTY(cvl) (cvl==CONDITION_LIST_EMPTY)

#define CLIST_Z     __C2CMASK(IS_ZERO)
#define CLIST_O     __C2CMASK(IS_ONE)
#define CLIST_F     __C2CMASK(IS_FULL)
#define CLIST_ZO    (CLIST_Z|CLIST_O)
#define CLIST_ZF    (CLIST_Z|CLIST_F)
#define CLIST_OF    (CLIST_O|CLIST_F)
#define CLIST_ZOF   (CLIST_Z|CLIST_O|CLIST_F)

#endif