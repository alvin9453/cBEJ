
#include <stdio.h>
#include <stdint.h>

typedef struct _parseinfo {
    uint8_t *buf;
    uint8_t *ptr;
    size_t datalen;
    size_t offset;
}ParseInfo_t;
typedef int(*ParseFunc) (ParseInfo_t*, void *data);
#define GETFUNC(TYPE) int get##TYPE(ParseInfo_t * psif, void *data)
#define FREEFUNC(TYPE) void free##TYPE(TYPE *data)
#define SHOWFUNC(TYPE) void show##TYPE(TYPE *data, uint8_t layer)

typedef struct _dicbasic
{
    uint8_t VersionTag;
    uint8_t truncation_flag:1;
    uint8_t DictionFlag_reserve:7;
    uint16_t EntryCount;
    uint32_t SchemaVersion;
    uint32_t DictionarySize;
} __attribute__((packed)) DictInfo_t;
GETFUNC(DictInfo_t);

typedef struct _entry
{
    uint8_t deferred_binding:1;
    uint8_t read_only_property:1;
    uint8_t nullable_property:1;
    uint8_t reserved:1;
    uint8_t bejtype:4;
    uint16_t SequenceNumber;
    uint16_t ChildPointerOffset;
    uint16_t ChildCount;
    uint8_t NameLength;
    uint16_t NameOffset;
} __attribute__((packed)) EntryParse_t;
GETFUNC(EntryParse_t);

typedef struct _entryinfo
{
    uint8_t deferred_binding;
    uint8_t read_only_property;
    uint8_t nullable_property;
    uint8_t bejtype;
    uint16_t SequenceNumber;
    struct _entryinfo *ChildInfo[64];
    uint16_t ChildCount;
    struct _entryinfo *ParentInfo;
    uint8_t *Name;
    uint8_t index;
} EntryInfo_t;


typedef struct _bejbasic {
    uint32_t ver32;
    uint16_t flags;
    uint8_t schemaclass;
} __attribute__((packed)) BejBasic_t;
GETFUNC(BejBasic_t);
SHOWFUNC(BejBasic_t);

typedef uint32_t nnint_t;
GETFUNC(nnint_t);
SHOWFUNC(nnint_t);

typedef struct _bejTupleS{
    nnint_t seq;
    uint8_t annot_flag;
    const char *name;
} BejTupleS_t;
#define NOSHOW "NOSHOW"
GETFUNC(BejTupleS_t);
SHOWFUNC(BejTupleS_t);


typedef struct _bejTupleF
{
    uint8_t deferred_binding:1;
    uint8_t read_only_property:1;
    uint8_t nullable_property:1;
    uint8_t reserved:1;
    uint8_t bejtype:4;
} __attribute__((packed)) BejTupleF_t;
GETFUNC(BejTupleF_t);
SHOWFUNC(BejTupleF_t);

typedef nnint_t BejTupleL_t;
GETFUNC(BejTupleL_t);
SHOWFUNC(BejTupleL_t);


typedef struct _bejTuple
{
    BejTupleS_t bejS;
    BejTupleF_t bejF;
    BejTupleL_t bejL;
    
    void *bejV;
    
} BejTuple_t;
FREEFUNC(BejTuple_t);
GETFUNC(BejTuple_t);
SHOWFUNC(BejTuple_t);

enum {
    bejSet,
    bejArray,
    bejNull,
    bejInteger,
    bejEnum,
    bejString,
    bejReal,
    bejBoolean,
    bejBytestring,
    bejChoice,
    bejPropertyAnnotation,
    bejreserve1,
    bejreserve2,
    bejreserve3,
    bejResourceLink,
    bejResourceLinkExpansion
};
#define ALL_BEJTYPE(MACRO) \
    MACRO(bejSet); \
    MACRO(bejArray); \
    MACRO(bejNull); \
    MACRO(bejInteger); \
    MACRO(bejEnum); \
    MACRO(bejString); \
    MACRO(bejReal); \
    MACRO(bejBoolean); \
    MACRO(bejBytestring); \
    MACRO(bejChoice); \
    MACRO(bejPropertyAnnotation); \
    MACRO(bejResourceLink); \
    MACRO(bejResourceLinkExpansion);

#define TYPEDEF(TYPE) typedef struct TYPE TYPE##_t
ALL_BEJTYPE(TYPEDEF);
#undef TYPEDEF
#define GETFUNC_t(TYPE) GETFUNC(TYPE##_t)
ALL_BEJTYPE(GETFUNC_t);
#undef GETFUNC_t
#define FREE_t(TYPE) FREEFUNC(TYPE##_t)
ALL_BEJTYPE(FREE_t);
#undef FREE_t
#define SHOW(TYPE) SHOWFUNC(TYPE##_t)
ALL_BEJTYPE(SHOW);
#undef SHOW

struct bejSet {
    nnint_t count;
    BejTuple_t * tuples;
};

struct bejArray {
    nnint_t count;
    BejTuple_t * tuples;
};

struct bejNull {
    uint8_t *nouse;
};

typedef long long bejint_t;
struct bejInteger {
    bejint_t value;
};

struct bejEnum {
    nnint_t nnint;
    const char *name;
};

struct bejString {
    uint8_t *strbuf;
};

struct bejReal {
    nnint_t wholelen;
    bejInteger_t whole;
    nnint_t fractlen;
    nnint_t fract;
    nnint_t explen;
    bejInteger_t exp;
};

struct bejBoolean {
    uint8_t value;
};

struct bejBytestring {
    size_t len;
    uint8_t *strbuf;
};

struct bejChoice {
    BejTuple_t tuple;
};

struct bejPropertyAnnotation {
    BejTuple_t annotuple;
};

struct bejResourceLink {
    nnint_t resourceId;
};

struct bejResourceLinkExpansion {
    nnint_t resourceId;
    BejBasic_t bejbasic;
    BejTuple_t bejtuple;
};


void parseDict(ParseInfo_t *parseinfo, EntryInfo_t **entryinfos_ptr);
int retrive(ParseInfo_t *parseInfo, void *data, ParseFunc fun);
void showEntries(EntryInfo_t*, uint8_t layer);

void parseBej(ParseInfo_t *parseinfo, BejTuple_t **bejtuple_ptr);

void resolveName(BejTuple_t *, EntryInfo_t*, EntryInfo_t *);
void resolveChildrenName(BejTuple_t *, EntryInfo_t*, EntryInfo_t *);
EntryInfo_t *lookupEntryInfoByTupleS(BejTupleS_t *, EntryInfo_t*, EntryInfo_t*);

ssize_t readFile(char *filename, uint8_t **input);
const char *getBejtypeName(uint8_t type);