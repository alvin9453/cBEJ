#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h> //for lexxtoh()
#include <limits.h>
#include <ctype.h>

#include "decodebej.h"
#include "cJSON.h"

#define UNUSED(x) (void)(x)
int DEBUG=1;


static inline void printBytes(uint8_t *input, size_t size)
{
    if(DEBUG){
        for(size_t i=0; i<size; i++)
            printf("0x%02x%s", input[i], ((i+1)%16)?" ":"\n");
    }
}

ssize_t readFile(char *filename, uint8_t **input)
{
    FILE *inputfile = NULL;
    size_t fsize=0;
    inputfile = fopen(filename, "r");
    if (inputfile==NULL) perror ("Error opening file");
    else {
        fseek(inputfile, 0, SEEK_END);
        fsize = (size_t) ftell(inputfile);
        rewind(inputfile);
        *input = (uint8_t*)malloc(sizeof(uint8_t) * fsize);
        // printf("input=%p\n", *input);
        if (NULL == *input){
            perror("malloc failed");
            return -1;
        }
        if (fsize != fread(*input, sizeof(uint8_t), fsize, inputfile)){
            perror("fread failed");
            return -2;
        }
    }
    return fsize;
}

#ifdef DECODE_TEST

int main(int argc, char* argv[])
{
    size_t fsize = 4096;
    uint8_t *input = NULL;
    EntryInfo_t *entryinfos;
    if(NULL!=getenv("DEBUG"))
        DEBUG = 1;
    if (argc < 2) {
        fprintf(stderr, "usage %s majordict [annodict] [bejpayload]\n", argv[0]);
        return 127;
    }
    else {
        ssize_t rtn= readFile(argv[1], &input);
        if(rtn<0) return (int) rtn;
        /*
        for (size_t i=0;i<rtn; i++) {
            printf("%02hhx%s", input[i], (i+1)%16?" ":"\n");
        }
        */
        printf("read %lx(=%lu) bytes\n", rtn, rtn);
        fsize = (size_t) rtn;
    }
    ParseInfo_t parseInfo = {.buf = input, .ptr = input,
        .datalen=fsize, .offset=0};
    parseDict(&parseInfo, &entryinfos);
    
    if(argc > 3) {
        // annodict + bejpayload
        uint8_t *annoinput;
        EntryInfo_t *annoinfos;
        ssize_t rtn= readFile(argv[2], &annoinput);
        if(rtn<0) return (int) rtn;
        ParseInfo_t parseanno = {.buf = annoinput, .ptr = annoinput,
            .datalen=(size_t)rtn, .offset=0};
        printf("read anno %lx(=%lu) bytes\n", rtn, rtn);
        parseDict(&parseanno, &annoinfos);

        uint8_t *bejinput;
        BejTuple_t *bejtuple;
        rtn= readFile(argv[3], &bejinput);
        ParseInfo_t parsebejinfo = {.buf = bejinput, .ptr = bejinput,
            .datalen=(size_t)rtn, .offset=0};
        parseBej(&parsebejinfo, &bejtuple);

        resolveName(bejtuple, entryinfos, annoinfos);

        showBejTuple_t(bejtuple, 0);
        puts("");

        free(annoinfos);
        free(annoinput);
        freeBejTuple_t(bejtuple);
        free(bejinput);
    }

    free(entryinfos);
    free(input);
    return 0;
}

#endif

const char *getBejtypeName(uint8_t type)
{
    switch(type)
    {
#define TP(T) case T: return #T;
        ALL_BEJTYPE(TP);
#undef TP
    }
    return "";
}


void parseDict(ParseInfo_t *parseinfo, EntryInfo_t **entryinfos_ptr)
{
    DictInfo_t basic;
    retrive(parseinfo, (void *)&basic, getDictInfo_t);
    // puts(" -> DictBasicInfo:");
    // printf("VersionTag: 0x%x(should be 0x0)\n", basic.VersionTag);
    // printf("truncation_flag: %u\n", basic.truncation_flag);
    // printf("EntryCount: %u\n", basic.EntryCount);
    // printf("SchemaVersion: %x\n", basic.SchemaVersion);
    // printf("DictionarySize: =%u\n", basic.DictionarySize);

    EntryParse_t entrybuf;
    EntryInfo_t *entryinfos = (EntryInfo_t*) malloc(sizeof(EntryInfo_t)* basic.EntryCount);
    if(NULL == entryinfos){
        perror("get entryinfo failed");
        exit(-1);
    }
    for(uint16_t i=0; i<basic.EntryCount; i++){
        retrive(parseinfo, &entrybuf, getEntryParse_t);
        /*
        printf(" -> EntryParse %u:\n", i);
        printf("Format: deferred binding=%u read only=%u nullable=%u %s\n",
                entrybuf.deferred_binding, entrybuf.read_only_property,
                entrybuf.nullable_property, getBejtypeName(entrybuf.bejtype));
        printf("SequenceNumber:%u\n", entrybuf.SequenceNumber);
        printf("ChildPointerOffset:%04x ", entrybuf.ChildPointerOffset);
        printf("ChildCount:%u\n", entrybuf.ChildCount);
        printf("NameLength:%u ", entrybuf.NameLength);
        printf("NameOffset:%04x ", entrybuf.NameOffset);
        printf(" -> \"%s\"\n", parseinfo->buf + entrybuf.NameOffset);
        */
        if(!i) entryinfos[i].ParentInfo = NULL;
        entryinfos[i].deferred_binding = entrybuf.deferred_binding;
        entryinfos[i].read_only_property = entrybuf.read_only_property;
        entryinfos[i].nullable_property = entrybuf.nullable_property;
        entryinfos[i].bejtype = entrybuf.bejtype;
        entryinfos[i].SequenceNumber = entrybuf.SequenceNumber;
        entryinfos[i].ChildCount = entrybuf.ChildCount;
        for(uint16_t count=0; count<entrybuf.ChildCount; count++){
            entryinfos[i].ChildInfo[count] = entryinfos + (entrybuf.ChildPointerOffset-sizeof(DictInfo_t))/sizeof(EntryParse_t)+count;
            //printf("offset %lu (%p)\n", (entrybuf.ChildPointerOffset-sizeof(DictInfo_t))/sizeof(EntryParse_t)+count, entryinfos[i].ChildInfo[count] );
            entryinfos[i].ChildInfo[count]->ParentInfo = &entryinfos[i];
        }
        if(entrybuf.NameOffset)
            entryinfos[i].Name = parseinfo->buf + entrybuf.NameOffset;
        else
            entryinfos[i].Name = (uint8_t *)"";
        entryinfos[i].index = i;
    }
    showEntries(entryinfos, 0);
    *entryinfos_ptr = entryinfos;
}

int retrive(ParseInfo_t *parseInfo, void *data, ParseFunc proc)
{
    int rtn = proc(parseInfo, data);
    return rtn;
}

#define PRINT_LAYER(layer) \
   for(uint8_t i=0; i<(layer); i++) printf("    ")
void showEntries(EntryInfo_t *info, uint8_t layer)
{
    PRINT_LAYER(layer);
    printf("%u: seq=%u -> "
           "\"%s\" %s"
           "%s %s %s", 
           info->index, info->SequenceNumber,
           info->Name, getBejtypeName(info->bejtype),
           info->deferred_binding?"df":"",
           info->read_only_property?"ro":"",
           info->nullable_property?"nu":"");
   printf(" ChildCount=%u\n", info->ChildCount);
   for(uint16_t i=0; i<(info->ChildCount); i++) {
       showEntries(info->ChildInfo[i], layer+1);
   }
}

void parseBej(ParseInfo_t *parseinfo, BejTuple_t ** bejtuple_ptr)
{
    BejTuple_t *bejtuple = (BejTuple_t*) malloc(sizeof(BejTuple_t));
    if(NULL == bejtuple) {
        perror("malloc bejtuple failed");
        exit(-2);
    }
    *bejtuple_ptr = bejtuple;
    BejBasic_t bejbasic;
    retrive(parseinfo, &bejbasic, getBejBasic_t);
    printf("Bej schmaclass: %d\n", bejbasic.schemaclass);

    retrive(parseinfo, bejtuple, getBejTuple_t);
    showBejTuple_t(bejtuple, 0);
    puts("");
}

void resolveName(BejTuple_t *bejtuple, EntryInfo_t *masterinfos, EntryInfo_t *annoinfos)
{
    //root tuple do not need name
    bejtuple->bejS.name = NOSHOW;
    //bejtuple->bejS.name = masterinfos->Name;
    resolveChildrenName(bejtuple, masterinfos, annoinfos);
}

void resolveChildrenName(BejTuple_t *tuple, EntryInfo_t *masterinfos,
        EntryInfo_t *annoinfos)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    BejTupleS_t *childS;
    BejTupleF_t *childF;
    BejTupleS_t enumS;
    EntryInfo_t *foundEntry;
    bejSet_t *vset;
    bejArray_t *varray;
    bejEnum_t *venum;
    bejPropertyAnnotation_t *vanno;
    nnint_t count;
    switch (bejF->bejtype)
    {
        case bejSet:
            vset = (bejSet_t*)tuple->bejV;
            count = vset->count;
            for(nnint_t i=0; i<count; i++){
                childS = &vset->tuples[i].bejS;
                foundEntry = lookupEntryInfoByTupleS(childS, masterinfos, annoinfos); 
                if(NULL != foundEntry) {
                    childS->name =(const char*) foundEntry->Name;
                    childF = &vset->tuples[i].bejF;
                    switch(childF->bejtype)
                    {
                        case bejSet:
                        case bejArray:
                        case bejEnum:
                        case bejPropertyAnnotation:
                            resolveChildrenName(&vset->tuples[i], foundEntry, annoinfos);
                    }
                }
            }
            break;
        case bejArray:
            varray = (bejArray_t*)tuple->bejV;
            count = varray->count;
            for(nnint_t i=0; i<count; i++){
                varray->tuples[i].bejS.name = NOSHOW;
                childF = &varray->tuples[i].bejF;
                switch(childF->bejtype)
                {
                    case bejSet:
                    case bejArray:
                    case bejEnum:
                    case bejPropertyAnnotation:
                        resolveChildrenName(&varray->tuples[i], masterinfos->ChildInfo[0],
                                annoinfos);
                }
            }
            break;
        case bejEnum:
            venum = (bejEnum_t*)tuple->bejV;
            enumS.seq = venum->nnint;
            enumS.annot_flag = 0;
            foundEntry = lookupEntryInfoByTupleS(&enumS, masterinfos, annoinfos); 
            if(NULL != foundEntry) {
                venum->name = (const char*)foundEntry->Name;
            }
            break;
        case bejPropertyAnnotation:
            vanno = (bejPropertyAnnotation_t*) tuple->bejV;
            childS = &vanno->annotuple.bejS;
            childF = &vanno->annotuple.bejF;
            foundEntry = lookupEntryInfoByTupleS(childS, masterinfos, annoinfos); 
            if(NULL != foundEntry) {
                bejS->name = (const char *)foundEntry->Name;
                childS->name = NOSHOW;
            }
            break;
        default:
            //do nothing
            break;
    }
}

EntryInfo_t *lookupEntryInfoByTupleS(BejTupleS_t *bejS, EntryInfo_t* parentinfo,
        EntryInfo_t*annoinfo)
{
    if(DEBUG) {
        printf(" %s() finding ", __func__);
        showBejTupleS_t(bejS, 1);
        printf(" in parentName =%s annoName=%s\n", parentinfo->Name, annoinfo->Name);
    }
    EntryInfo_t *pool=bejS->annot_flag?annoinfo:parentinfo;
    for(uint16_t i=0; i<pool->ChildCount; i++)
    {
        if(bejS->seq == (nnint_t)pool->ChildInfo[i]->SequenceNumber)
            return pool->ChildInfo[i];
    }
    
    return NULL;
}


#define CHK_LEN(psif, size) \
    if((psif->datalen - psif->offset)< (size) ){\
        fprintf(stderr, "out of range!");\
        return -1;\
    }

#define GETPTR(psif) (psif->ptr)

#define CONSUME(psif, size) do{\
    psif->offset += size;\
    psif->ptr+=size;\
}while(0)


#define DECLEAR_GETTYPE(TYPE) \
    GETFUNC(TYPE) \
{\
    CHK_LEN(psif, sizeof(TYPE)); \
    TYPE *value = (TYPE*)data;\
    uint8_t *ptr = GETPTR(psif);\
    *value = *(TYPE*) ptr;\
    printBytes(ptr, sizeof(TYPE));\
    CONSUME(psif, sizeof(TYPE));\
    return 0;\
}

DECLEAR_GETTYPE(uint32_t)
DECLEAR_GETTYPE(uint16_t)
DECLEAR_GETTYPE(uint8_t)

DECLEAR_GETTYPE(DictInfo_t);

DECLEAR_GETTYPE(EntryParse_t);

DECLEAR_GETTYPE(BejBasic_t);

GETFUNC(BejTuple_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    BejTuple_t *tuple = (BejTuple_t*) data;
    retrive(psif, &tuple->bejS, getBejTupleS_t);
    retrive(psif, &tuple->bejF, getBejTupleF_t);
    retrive(psif, &tuple->bejL, getBejTupleL_t);

    uint8_t *ptr = GETPTR(psif);
    ParseInfo_t subparseinfo = {.buf=ptr, .ptr=ptr,
        .datalen = (size_t)tuple->bejL, .offset = 0};
    if(DEBUG) {
        printBytes(ptr, tuple->bejL);
        printf(" -> (retrive %s)\n", getBejtypeName(tuple->bejF.bejtype));
    }
    switch(tuple->bejF.bejtype)
    {
#define TP(X) \
        case X: \
            tuple->bejV  = (void *) calloc(1,sizeof(X##_t));\
            if(NULL == tuple->bejV) { \
                perror("malloc" #X "_t error");\
                exit(-3); \
            }\
            retrive(&subparseinfo, tuple->bejV, get##X##_t);\
            break;
        ALL_BEJTYPE(TP);
#undef TP
    
    }
    CONSUME(psif, tuple->bejL);

    return 0;
}

void freeSingleBejTuple_t(BejTuple_t *tuple)
{
    switch(tuple->bejF.bejtype)
    {
#define TP(X) \
        case X: \
            free##X##_t((X##_t *)tuple->bejV);\
            break;
        ALL_BEJTYPE(TP);
#undef TP
    }
}

FREEFUNC(BejTuple_t)
{
    BejTuple_t *tuple = data;
    freeSingleBejTuple_t(tuple);
    free(tuple);
}

SHOWFUNC(BejTuple_t)
{
    //if(DEBUG) printf("in %s()..\n", __func__);
    PRINT_LAYER(layer);
    BejTuple_t *tuple = data;
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    char bejbasicbuf[128];
    
    if(NULL==bejS->name) {
        snprintf(bejbasicbuf, sizeof(bejbasicbuf),  "(seq=%s%u,%s%s,%u):",
                bejS->annot_flag?"an":"", bejS->seq,
                getBejtypeName(bejF->bejtype),
                bejF->deferred_binding?"(def)":"", tuple->bejL); 
    }
    else if(!strncmp(bejS->name, NOSHOW, 10))
        bejbasicbuf[0]=0;
    else {
        snprintf(bejbasicbuf, sizeof(bejbasicbuf),  "\"%s\":",bejS->name);
    }

    switch(bejF->bejtype)
    {
#define TP(X) case X: \
        printf("%s", bejbasicbuf);\
        show##X##_t(tuple->bejV, layer+1);\
        break;
        ALL_BEJTYPE(TP);
#undef TP
    }
}

GETFUNC(nnint_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    nnint_t *nnint = (nnint_t*)data;
    uint8_t len;
    retrive(psif, &len, getuint8_t);
    if(DEBUG) puts(" -> (nnint size)");
    if(len > sizeof(nnint_t)) {
        fprintf(stderr, "Nnint bigger than %u bytes!!\n", len);
        exit(-4);
    }

    CHK_LEN(psif, len);
    *nnint = 0;
    uint8_t *ptr = GETPTR(psif);
    printBytes(ptr, len);
    for (uint8_t i=0; i<len; i++) {
        *nnint |= (ptr[i] << (i*8));
        CONSUME(psif, 1);
    }
    if(DEBUG) {
        printf(" -> (nnint) =%d\n", *nnint);
    }
    return 0;
}

GETFUNC(BejTupleS_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    BejTupleS_t * bejS = (BejTupleS_t*) data;
    nnint_t nnint;
    retrive(psif, &nnint, getnnint_t);
    bejS->annot_flag = ((uint8_t) nnint) & 0x01;
    bejS->seq = nnint >> 1;
    if(DEBUG) {
        showBejTupleS_t(bejS, 0);
    }
    return 0;
}

SHOWFUNC(BejTupleS_t)
{
    UNUSED(layer);
    printf(" => annot_flag=%u seq=%u\n", data->annot_flag, data->seq);
}

GETFUNC(BejTupleF_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    retrive(psif, data, getuint8_t);
    BejTupleF_t * bejF = (BejTupleF_t*) data;
    if(DEBUG) {
        showBejTupleF_t(bejF, 0);
    }
    return 0;
}

SHOWFUNC(BejTupleF_t)
{
    UNUSED(layer);
    printf(" => def=%u bytes=%u(%s)\n", data->deferred_binding, data->bejtype,
            getBejtypeName(data->bejtype));
}

GETFUNC(BejTupleL_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    retrive(psif, data, getnnint_t);
    BejTupleL_t * bejL = (BejTupleL_t*) data;
    if(DEBUG) {
        printf(" => (bejL) =%u\n", *bejL);
    }
    return 0;
}

GETFUNC(bejSet_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejSet_t *bejset = (bejSet_t*) data;
    retrive(psif, &bejset->count, getnnint_t);
    nnint_t count = bejset->count;
    if(DEBUG) printf(" -> (bejset->count) =%u\n", count);


    bejset->tuples = (BejTuple_t*) calloc(count, sizeof(BejTuple_t));
    if(NULL==bejset->tuples) {
        perror("malloc bejset tuples failed");
        exit(-4);
    }
    ParseInfo_t subparseinfo = {.buf=GETPTR(psif), .ptr=GETPTR(psif),
        .datalen =psif->datalen-psif->offset, .offset=0};
    for(nnint_t i=0; i<count; i++){
        if(DEBUG) printf(" ---- retrive subset %d ----\n", i);
        retrive(&subparseinfo, (void*)&bejset->tuples[i], getBejTuple_t);
        //if(DEBUG) showBejTuple_t(&bejset->tuples[i], 0);
    }
    CONSUME(psif, count);

    return 0;
}

FREEFUNC(bejSet_t)
{
    bejSet_t *bejset = (bejSet_t*) data;
    for(nnint_t i=0; i<bejset->count; i++)
    {
        freeSingleBejTuple_t(&bejset->tuples[i]);
    }
    free(bejset->tuples);
    return ;
}

SHOWFUNC(bejSet_t)
{
    //if(DEBUG) printf("in %s()..\n", __func__);
    //puts("");
    //PRINT_LAYER(layer-1);
    printf("{");
    if(data->count) puts("");
    for(nnint_t i=0; i<data->count;i++){
        showBejTuple_t(&data->tuples[i], layer);
        if(i != (data->count -1)) {
            printf(",");
        }
        puts("");
    }
    if(data->count) {
        PRINT_LAYER(layer-1);
    }
    printf("}");
}

GETFUNC(bejArray_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    return getbejSet_t(psif, data);
}

FREEFUNC(bejArray_t)
{
    freebejSet_t((bejSet_t*)data);
}

SHOWFUNC(bejArray_t)
{
    //puts("");
    //PRINT_LAYER(layer-1);
    printf("[");
    if(data->count) puts("");
    for(nnint_t i=0; i<data->count;i++){
        showBejTuple_t(&data->tuples[i], layer);
        if(i != (data->count -1)) {
            puts(",");
        }
    }
    if(data->count){
        puts("");
        PRINT_LAYER(layer-1);
    }
    printf("]");
}

GETFUNC(bejNull_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejNull_t *bej = (bejNull_t*) data;
    bej->nouse = psif->buf;
    return 0;
}

FREEFUNC(bejNull_t)
{
    free(data);
}

SHOWFUNC(bejNull_t)
{
    UNUSED(data); UNUSED(layer);
    printf("null");
}

GETFUNC(bejInteger_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    size_t len = psif->datalen;
    bejInteger_t *bejint = (bejInteger_t *) data;
    if(len > sizeof(bejint_t)) {
        fprintf(stderr, "size of bejInteger not enough %zu > %zu\n",
                len, sizeof(bejint_t));
        len = sizeof(bejint_t);
    }
    uint8_t *ptr = GETPTR(psif);
    printBytes(ptr, len);
    bejint->value = 0;
    if (len >0) {
        if (len == sizeof(int8_t)){
            bejint->value = (bejint_t) *(int8_t*)ptr;
            CONSUME(psif, sizeof(int8_t));
        }
        else if (len == sizeof(int16_t)){
            bejint->value = (bejint_t) *(int16_t*)ptr;
            CONSUME(psif, sizeof(int16_t));
        }
        else if (len == sizeof(int32_t)){
            bejint->value = (bejint_t) *(int32_t*)ptr;
            CONSUME(psif, sizeof(int32_t));
        }
        else if (len == sizeof(int64_t)){
            bejint->value = (bejint_t) *(int64_t*)ptr;
            CONSUME(psif, sizeof(int64_t));
        }
        else {
            uint8_t signbit = ptr[len-1] & 0x80; 
            if(signbit) {
                //negtive
                for (size_t i=0; i<len; i++) {
                    bejint->value |=(bejint_t)((uint8_t)~ptr[i]) << (i*8);
                    CONSUME(psif, 1);
                }
                bejint->value = ~bejint->value;
            }
            else {
                //positive
                for (size_t i=0; i<len; i++) {
                    bejint->value |=(bejint_t)(ptr[i] << (i*8));
                    CONSUME(psif, 1);
                }
            }
        }
    }
    if(DEBUG) printf(" -> (bejInteger) =%lld\n", bejint->value);
    return 0;
}

FREEFUNC(bejInteger_t)
{
    free(data);
}

SHOWFUNC(bejInteger_t)
{
    UNUSED(layer);
    printf("%lld", data->value);
}

GETFUNC(bejEnum_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejEnum_t *bejenum = (bejEnum_t*)data;
    retrive(psif, (void*)&bejenum->nnint, getnnint_t);
    if(DEBUG) showbejEnum_t(bejenum, 0);
    return 0;
}

FREEFUNC(bejEnum_t)
{
    free(data);
}

SHOWFUNC(bejEnum_t)
{
    UNUSED(layer);
    if(data->name)
        printf("\"%s\"", data->name);
    else
        printf("(enum %u)", data->nnint);
}

GETFUNC(bejString_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejString_t *bejstring = (bejString_t *)data;
    bejstring->strbuf = psif->ptr;
    size_t len = strnlen((const char*)bejstring->strbuf, psif->datalen);

    if(len!= (psif->datalen-1)) {
        fprintf(stderr, "bejString len error %zu != %zu\n", len, psif->datalen -1);
    }
    printBytes(psif->ptr, psif->datalen);
    CONSUME(psif, psif->datalen);
    if(DEBUG) showbejString_t(bejstring, 0);
    return 0;
}

FREEFUNC(bejString_t)
{
    free(data);
}

SHOWFUNC(bejString_t)
{
    UNUSED(layer);
    printf("\"%s\"", data->strbuf);
}

GETFUNC(bejReal_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejReal_t *real = (bejReal_t*)data;
    retrive(psif, &real->wholelen, getnnint_t);
    size_t len = (size_t) real->wholelen;
    if(DEBUG) printf(" -> (bejReal wholelen) =%zu\n", len);

    ParseInfo_t subparseinfo = {.buf=GETPTR(psif),.ptr=GETPTR(psif),
        .datalen = len, .offset=0 };
    retrive(&subparseinfo, &real->whole, getbejInteger_t);
    CONSUME(psif, len);
    if(DEBUG) printf(" -> (bejReal whole) =%lld\n", real->whole.value);

    retrive(psif, &real->fractlen, getnnint_t);
    if(DEBUG) printf(" -> (bejReal fractlen) =%u\n", real->fractlen);
    retrive(psif, &real->fract, getnnint_t);
    if(DEBUG) printf(" -> (bejReal fract) =%u\n", real->fract);

    retrive(psif, &real->explen, getnnint_t);
    if(DEBUG) printf(" -> (bejReal explen) =%u\n", real->explen);
    len = (size_t)real->explen;
    subparseinfo.buf = GETPTR(psif);
    subparseinfo.ptr = GETPTR(psif);
    subparseinfo.datalen = len;
    subparseinfo.offset = 0;
    retrive(&subparseinfo, &real->exp, getbejInteger_t);
    CONSUME(psif, len);
    if(DEBUG) printf(" -> (bejReal exp) =%lld\n", real->exp.value);

    return 0;
}

FREEFUNC(bejReal_t)
{
    free(data);
}

SHOWFUNC(bejReal_t)
{
    UNUSED(layer);
    printf("%lld.%uE%lld", data->whole.value, data->fract, data->exp.value);
}

GETFUNC(bejBoolean_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejBoolean_t *bool = (bejBoolean_t*)data;
    retrive(psif, &bool->value, getuint8_t);
    if(DEBUG) showbejBoolean_t(bool, 0);
    return 0;
}

FREEFUNC(bejBoolean_t)
{
    free(data);
}

SHOWFUNC(bejBoolean_t)
{
    UNUSED(layer);
    printf("%s", data->value?"true":"false");
}

GETFUNC(bejBytestring_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejBytestring_t *str = (bejBytestring_t*)data;
    str->strbuf = psif->ptr;
    str->len = psif->datalen;
    CONSUME(psif, psif->datalen);
    if(DEBUG) showbejBytestring_t(str, 0);
    return 0;
}

FREEFUNC(bejBytestring_t)
{
    free(data);
}

SHOWFUNC(bejBytestring_t)
{
    UNUSED(layer);
    for(size_t i=0; i<data->len; i++)
        printf("0x%02x%s", data->strbuf[i], ((i+1)%16)?" ":"\n");
}

GETFUNC(bejChoice_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejChoice_t *choice = (bejChoice_t*)data;
    return retrive(psif, &choice->tuple, getBejTuple_t);
}

FREEFUNC(bejChoice_t)
{
    bejChoice_t *choice = (bejChoice_t*)data;
    freeSingleBejTuple_t(&choice->tuple);
    free(data);
}

SHOWFUNC(bejChoice_t)
{
    showBejTuple_t(&data->tuple, layer+1);
}

GETFUNC(bejPropertyAnnotation_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejPropertyAnnotation_t *anno = (bejPropertyAnnotation_t*)data;
    return retrive(psif, &anno->annotuple, getBejTuple_t);
}

FREEFUNC(bejPropertyAnnotation_t)
{
    bejPropertyAnnotation_t *anno = (bejPropertyAnnotation_t*)data;
    freeSingleBejTuple_t(&anno->annotuple);
    free(anno);
}

SHOWFUNC(bejPropertyAnnotation_t)
{
    UNUSED(layer);
    showBejTuple_t(&data->annotuple, 0);
}

GETFUNC(bejResourceLink_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejResourceLink_t *link = (bejResourceLink_t*)data;
    retrive(psif, &link->resourceId, getnnint_t);
    if(DEBUG) printf(" -> (Resourcelink) %u\n", link->resourceId);
    return 0;
}

FREEFUNC(bejResourceLink_t)
{
    free(data);
}

SHOWFUNC(bejResourceLink_t)
{
    UNUSED(layer);
    printf("\"%%L%u\"", data->resourceId);
}

GETFUNC(bejResourceLinkExpansion_t)
{
    if(DEBUG) printf("in %s()..\n", __func__);
    bejResourceLinkExpansion_t *expansion = (bejResourceLinkExpansion_t*)data;
    retrive(psif, &expansion->resourceId, getnnint_t);
    retrive(psif, &expansion->bejbasic, getBejBasic_t);
    retrive(psif, &expansion->bejtuple, getBejTuple_t);
    return 0;
}

FREEFUNC(bejResourceLinkExpansion_t)
{
    bejResourceLinkExpansion_t *expansion = (bejResourceLinkExpansion_t*)data;
    freeSingleBejTuple_t(&expansion->bejtuple);
    free(data);
}

SHOWFUNC(bejResourceLinkExpansion_t)
{
    UNUSED(layer);
    printf("<ResourceID %u> ", data->resourceId);
    showBejTuple_t(&data->bejtuple, layer+1);
}
