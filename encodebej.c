#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encodebej.h"

extern int DEBUG;
#define ENCODE_TEST 1

#ifdef ENCODE_TEST

#define PRINT_LAYER(layer) \
   for(uint8_t i=0; i<(layer); i++) printf("    ")

EntryInfo_t *find_entry_from_dictionary(char *name, EntryInfo_t *dict)
{
    if (name == NULL)
        return NULL;

    EntryInfo_t *found_entry = NULL;
    for (uint16_t i = 0; i < dict->ChildCount; i++)
    {
        if (strcmp(name, dict->ChildInfo[i]->Name) == 0)
        {
            found_entry = dict->ChildInfo[i];
            break;
        }
        else if (dict->ChildInfo[i]->ChildCount > 0)
        {
            found_entry = find_entry_from_dictionary(name, dict->ChildInfo[i]);
            if (found_entry != NULL)
                break;
        }
    }
    return found_entry;
}

// nnint => bytes : value
// Convert raw value(e.g. real BejS seq number or real BejL) to nnint_t foramt
// e.g. if nnint is 4 bytes, then 0x03 0x00 0x00 0x41 value is 65; 0x03 0x00 0x39 0x05 is 1337
nnint_t convert_to_nnint_format(nnint_t raw)
{
    nnint_t value = sizeof(nnint_t) - 1;
    value <<= 8 * (sizeof(nnint_t) - 1);

    if (raw >= value)
    {
        printf(" - [ERROR] too many bytes for fixed nnint\n");
        return 0;
    }

    value |= raw;

    return value;
}

nnint_t get_value_from_nnint_format(nnint_t nnint)
{
    nnint_t count;
    unsigned int mask = 0xff;
    for(uint8_t i = 1; i < sizeof(nnint_t) - 1; i++)
    {
        mask <<= 8;
        mask |= 0xff;
    }
    // printf("    - mask = %x , nnint & mask = %x\n", mask, nnint & mask);
    return nnint & mask;
}


BejTuple_t *pack_json_to_sfv(const cJSON *json, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    // parse cjson data to bej_tuple
    // but still not fill the length

    BejTupleS_t bejS;
    BejTupleF_t bejF;
    void *bejV;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *bej_integer;
    bejBoolean_t *bej_bool;
    BejTuple_t *varray_tuples_p;
    BejTuple_t *vset_tuples_p;
    BejTuple_t *packed_result;
    int index = 0;
    // printf(" -- %s\n", json->string == NULL ? "<SET>" : json->string) ;

    const cJSON *obj = NULL;

    // BejV
    switch (json->type)
    {
    case cJSON_Object:
        vset = malloc(sizeof(bejSet_t));
        vset->count = cJSON_GetArraySize(json);
        if (cJSON_GetArraySize(json) != 0)
        {
            vset->tuples = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
            vset_tuples_p = vset->tuples;

            cJSON_ArrayForEach(obj, json)
            {
                packed_result = pack_json_to_sfv(obj, major_dict, annotation_dict);
                if (packed_result != NULL)
                {
                    memcpy(&vset->tuples[index++], packed_result, sizeof(*packed_result));
                }
            }
            vset->tuples = vset_tuples_p;
        }
        else
        {
            vset->tuples = NULL;
        }
        bejV = vset;
        break;
    case cJSON_Array:
        varray = malloc(sizeof(bejArray_t));
        varray->tuples = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
        varray->count = cJSON_GetArraySize(json);
        varray_tuples_p = varray->tuples;
        cJSON_ArrayForEach(obj, json)
        {
            packed_result = pack_json_to_sfv(obj, major_dict, annotation_dict);
            memcpy(&varray->tuples[index++], packed_result, sizeof(BejTuple_t));
        }
        varray->tuples = varray_tuples_p;
        bejV = varray;
        break;
    case cJSON_String:
        bejV = malloc(strlen(json->valuestring));
        memcpy(bejV, (char *)json->valuestring, strlen(json->valuestring) + 1);
        break;
    case cJSON_Number:
        bej_integer = malloc(sizeof(bejInteger_t));
        bej_integer->value = (bejint_t)json->valuedouble;
        bejV = bej_integer;
        break;
    case cJSON_True:
        bej_bool = malloc(sizeof(bejBoolean_t));
        bej_bool->value = 0x00;
        bejV = bej_bool;
        break;
    case cJSON_False:
        bej_bool = malloc(sizeof(bejBoolean_t));
        bej_bool->value = 0xff;
        bejV = bej_bool;
        break;
    case cJSON_NULL:
        bejV = NULL;
        break;
    default:
        break;
    }

    if(json->string == NULL) 
    {
        bejS.seq = 0;
        bejS.name = "";
        bejS.annot_flag = 0;

        bejF.bejtype = bejSet;
    }
    else
    {
        EntryInfo_t *find_major_entry = find_entry_from_dictionary(json->string, major_dict);
        if (find_major_entry != NULL)
        {
            // printf(" - [DEBUG] find \"%s\" in dictionary , seq number = %u\n", find_major_entry->Name, find_major_entry->SequenceNumber);
            bejS.seq = find_major_entry->SequenceNumber;
            bejS.name = find_major_entry->Name;
            bejS.annot_flag = 0;

            // BejTupleF_t
            // printf(" - [DEBUG] bejtype = %s\n", getBejtypeName(find_major_entry->bejtype));
            bejF.bejtype = find_major_entry->bejtype;
        }

        // Set annotation flag
        EntryInfo_t *find_annotation_entry = find_entry_from_dictionary(json->string, annotation_dict);
        if (find_annotation_entry != NULL)
        {
            // printf(" - [DEBUG] find \"%s\" is annotation. \n", find_annotation_entry->Name);
            bejS.seq = find_annotation_entry->SequenceNumber;
            bejS.name = find_annotation_entry->Name;
            bejS.annot_flag = 1;

            // printf(" - [DEBUG] bejtype = %s\n", getBejtypeName(find_annotation_entry->bejtype));

            bejF.bejtype = find_annotation_entry->bejtype;
        }

        if (find_major_entry == NULL && find_annotation_entry == NULL)
        {
            printf(" - !!!! [DEBUG] ERROR, cannot find \"%s\" property in dictionary.\n", json->string);
            return NULL;
        }
    }

    BejTuple_t *bej_tuple = malloc(sizeof(BejTuple_t));
    bej_tuple->bejS = bejS;
    bej_tuple->bejF = bejF;
    bej_tuple->bejV = bejV;

    // printf(" [DEBUG] seq = %d, name = %s, annotation_flag = %d, type = %s\n", bejS.seq, bejS.name, bejS.annot_flag, getBejtypeName(bejF.bejtype));

    return bej_tuple;

}

// Todo : remove layer which is for debug
nnint_t fill_tuple_length(BejTuple_t *tuple, int layer)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    BejTupleL_t bejL = 0;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *vinteger;
    bejString_t *vstring;
    bejBoolean_t *vbool;
    nnint_t count = 0;
    BejTuple_t *bejtuple;
    nnint_t total_bej_tuple_slvf_length = 0;
    nnint_t bejv_length = 0;

    PRINT_LAYER(layer);
    switch (bejF->bejtype)
    {
    case bejSet:
    
        
        vset = (bejSet_t *)tuple->bejV;
        if (vset != NULL)
        {
            count = vset->count;
            for (nnint_t i = 0; i < count; i++)
            {
                bejtuple = &vset->tuples[i];
                bejv_length += fill_tuple_length(bejtuple, layer + 1) + sizeof(nnint_t) + 1;
            }
            bejL = bejv_length;
        }
        printf(" - [0x%x] %s , \"bejSet\", bejL = 0x%x\n", bejS->seq, bejS->name == "" ? "<SET>" : bejS->name, bejL);
        break;
    case bejArray:
        varray = (bejArray_t *)tuple->bejV;
        count = varray->count;
        for (nnint_t i = 0; i < count; i++)
        {
            bejtuple = &varray->tuples[i];
            bejv_length += fill_tuple_length(bejtuple, layer + 1);
        }
        bejL = bejv_length;
        printf(" - [0x%x] %s , \"bejArray\", bejL = 0x%x\n", bejS->seq, bejS->name, bejL);
        break;
    case bejString:
        vstring = (bejString_t *)tuple->bejV;
        bejv_length = strlen((const char *)vstring) + 1;
        bejL = bejv_length;
        tuple->bejL = bejL;
        printf(" - [0x%x] %s , \"bejString\", value = %s, strlen = %ld, bejL = 0x%x\n", bejS->seq, bejS->name, strlen((const char *)vstring) > 1 ? (const char *)vstring : "\"\"", strlen((const char *)vstring), bejL);
        break;
    case bejInteger:
        vinteger = (bejInteger_t *)tuple->bejV;
        bejv_length = sizeof(vinteger->value);
        bejL = bejv_length;
        tuple->bejL = bejL;
        printf(" - [0x%x] %s , \"bejInteger\", value = %lld, bejL = 0x%x\n", bejS->seq, bejS->name, vinteger->value, bejL);
        break;
    case bejEnum:
        bejv_length = (tuple->bejV == NULL ? 0 : strlen((const char *)tuple->bejV) + 1);
        bejL = bejv_length;
        tuple->bejL = bejL;
        printf(" - [0x%x] %s , \"bejEnum\", value = %s, bejL = 0x%x\n", bejS->seq, bejS->name, tuple->bejV == NULL ? "null" : (const char *)tuple->bejV, bejL);
        break;
    case bejBoolean:
        vbool = (bejBoolean_t *)tuple->bejV;
        bejv_length = sizeof(vbool->value);
        bejL = bejv_length;
        tuple->bejL = bejL;
        printf(" - [0x%x] %s , \"bejBoolean\", value = %s , bejL = 0x%x\n", bejS->seq, bejS->name, vbool->value == 0x00 ? "true" : "false", bejL);
        break;
    default:
        // TODO : the other BEJ type
        // bejBytesString
        // bejChoice
        // bejPropertyAnnotation
        // bejResourceLink
        // bejResourceLinkExpansion
        // bejReal
        break;
    }
    if( strncmp(bejS->name, "root_tuple", strlen("root_tuple")) == 0){
        return bejv_length;
    }else{
        total_bej_tuple_slvf_length = sizeof(bejS->seq) + sizeof(BejTupleF_t) + sizeof(bejL) + bejv_length + 2; // add 2 bytes for BejS and BejL nnint
        return total_bej_tuple_slvf_length;
    }
}

void showTuple(BejTuple_t *tuple, int layer)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *vinteger;
    bejString_t *vstring;
    bejBoolean_t *vbool;
    nnint_t count;
    BejTuple_t *bejtuple;

    PRINT_LAYER(layer);
    switch (bejF->bejtype)
    {
    case bejSet:
        printf(" - [%d] %s , \"bejSet\", annotation_flag = %d\n", bejS->seq, bejS->name == "" ? "<SET>" : bejS->name, bejS->annot_flag);
        vset = (bejSet_t *)tuple->bejV;
        if (vset != NULL)
        {
            count = vset->count;
            for (nnint_t i = 0; i < count; i++)
            {
                bejtuple = &vset->tuples[i];
                showTuple(bejtuple, layer + 1);
            }
        }
        break;
    case bejArray:
        printf(" - [%d] %s , \"bejArray\", annotation_flag = %d\n", bejS->seq, bejS->name, bejS->annot_flag);
        varray = (bejArray_t *)tuple->bejV;
        count = varray->count;
        for (nnint_t i = 0; i < count; i++)
        {
            bejtuple = &varray->tuples[i];
            showTuple(bejtuple, layer + 1);
        }
        break;
    case bejString:
        vstring = (bejString_t *)tuple->bejV;
        printf(" - [%d] %s , \"bejString\", value = %s\n", bejS->seq, bejS->name, strlen((const char *)vstring) > 1 ? (const char *)vstring : "\"\"");
        break;
    case bejInteger:
        vinteger = (bejInteger_t *)tuple->bejV;
        printf(" - [%d] %s , \"bejInteger\", value = %lld\n", bejS->seq, bejS->name, vinteger->value);
        break;
    case bejEnum:
        printf(" - [%d] %s , \"bejEnum\", value = %s\n", bejS->seq, bejS->name, tuple->bejV == NULL ? "null" : (const char *)tuple->bejV);
        break;
    case bejBoolean:
        vbool = (bejBoolean_t *)tuple->bejV;
        printf(" - [%d] %s , \"bejBoolean\", value = %s\n", bejS->seq, bejS->name, vbool->value == 0x00 ? "true" : "false");
        break;
    default:
        // TODO : the other BEJ type
        // bejBytesString
        // bejChoice
        // bejPropertyAnnotation
        // bejResourceLink
        // bejResourceLinkExpansion
        // bejReal
        break;
    }
}

void print_bytes(uint8_t *input, size_t size)
{
    for (size_t i = 0; i < size; i++)
        printf("0x%02x%s", input[i], ((i + 1) % 16) ? " " : "\n");
}

void showBejTupleInBytes(BejTuple_t *tuple)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    BejTupleL_t *bejL = &tuple->bejL;

    bejSet_t *vset;
    bejArray_t *varray;
    nnint_t count = 0;
    FILE *fileToWrite = NULL;
    uint8_t nnint_size = sizeof(nnint_t);
    nnint_t annotation_flag = bejS->annot_flag;
    nnint_t seq = bejS->seq << 1 + annotation_flag;

    if ((fileToWrite = fopen("bejencode_result.bin", "ab+")) != NULL)
    {
        // Write BejS
        fwrite(&nnint_size, sizeof(uint8_t), 1, fileToWrite);
        fwrite(&seq, sizeof(nnint_t), 1, fileToWrite);

        // Write BejF
        fwrite(bejF, sizeof(BejTupleF_t), 1, fileToWrite);

        // Write BejL
        fwrite(&nnint_size, sizeof(uint8_t), 1, fileToWrite);
        fwrite(bejL, sizeof(BejTupleL_t),1, fileToWrite);

        switch (bejF->bejtype)
        {
        case bejSet:
            vset = (bejSet_t *)tuple->bejV;
            count = vset->count;

            fwrite(&nnint_size, sizeof(uint8_t), 1, fileToWrite);
            fwrite(&count, sizeof(nnint_t),1, fileToWrite);
            break;
        case bejArray:
            varray = (bejArray_t *)tuple->bejV;
            count = varray->count;

            fwrite(&nnint_size, sizeof(uint8_t), 1, fileToWrite);
            fwrite(&count, sizeof(nnint_t), 1, fileToWrite);
            break;
        default:
            fwrite(tuple->bejV, *bejL, 1, fileToWrite);
            break;
        }
        fclose(fileToWrite);
    }
}

void showEncodedBejResult(BejTuple_t *tuple)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *vinteger;
    bejString_t *vstring;
    bejBoolean_t *vbool;
    nnint_t count;
    BejTuple_t *bejtuple;

    switch (bejF->bejtype)
    {
    case bejSet:
        showBejTupleInBytes(tuple);
        vset = (bejSet_t *)tuple->bejV;
        if (vset != NULL)
        {
            count = vset->count;
            for (nnint_t i = 0; i < count; i++)
            {
                bejtuple = &vset->tuples[i];
                showEncodedBejResult(bejtuple);
            }
        }
        break;
    case bejArray:
        showBejTupleInBytes(tuple);
        varray = (bejArray_t *)tuple->bejV;
        count = varray->count;
        for (nnint_t i = 0; i < count; i++)
        {
            bejtuple = &varray->tuples[i];
            showEncodedBejResult(bejtuple);
        }
        break;
    case bejString:
    case bejInteger:
    case bejEnum:
    case bejBoolean:
    default:
        showBejTupleInBytes(tuple);
        // TODO : the other BEJ type
        // bejBytesString
        // bejChoice
        // bejPropertyAnnotation
        // bejResourceLink
        // bejResourceLinkExpansion
        // bejReal
        break;
    }
}

void encodeJsonToBinary(BejTuple_t *bej_tuple_list, cJSON *json_input, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    BejBasic_t bej_basic = {.ver32 = 0xF1F0F000, .flags = 0, .schemaclass = 0};

    const cJSON *obj = NULL;

    // Create root tuple
    BejTupleS_t bejS = {.seq = 0, .annot_flag = 0, .name = "root_tuple"};
    BejTupleF_t bejF = {.bejtype = bejSet};
    BejTuple_t *root_bej_tuple = malloc(sizeof(BejTuple_t));
    root_bej_tuple->bejS = bejS;
    root_bej_tuple->bejF = bejF;

    BejTuple_t *root_bej_V = malloc(cJSON_GetArraySize(json_input) * sizeof(BejTuple_t));
    BejTuple_t *root_bej_V_init_ptr;
    root_bej_V_init_ptr = root_bej_V;
    bejSet_t *root_bej_set = malloc(sizeof(bejSet_t));

    cJSON_ArrayForEach(obj, json_input)
    {
        BejTuple_t *tuple = pack_json_to_sfv(obj, major_dict, annotation_dict);
        // printf(" [DEBUG] seq = %d, name = %s, annotation_flag = %d, type = %s\n", tuple->bejS.seq, tuple->bejS.name, tuple->bejS.annot_flag, getBejtypeName(tuple->bejF.bejtype));
        memcpy(root_bej_V_init_ptr, tuple, sizeof(BejTuple_t));

        root_bej_V_init_ptr++;
    }
    root_bej_set->tuples = malloc(sizeof(BejTuple_t));
    root_bej_set->tuples = root_bej_V;
    root_bej_set->count = cJSON_GetArraySize(json_input);

    root_bej_tuple->bejV = root_bej_set;

    printf("------------------------------------------\n");
    // showTuple(root_bej_tuple, 0);

    nnint_t bejv_length = 0;
    bejv_length = fill_tuple_length(root_bej_tuple, 0);
    root_bej_tuple->bejL = bejv_length;

    FILE *fileToWrite = NULL;
    if ((fileToWrite = fopen("bejencode_result.txt", "ab+")) != NULL)
    {
        fwrite(&bej_basic, sizeof(bej_basic), 1, fileToWrite);
    }
    fclose(fileToWrite);

    showEncodedBejResult(root_bej_tuple);

    free(root_bej_set);
    // Traverse the Linked list and calculate the length

    // Show BEJ encoded result
}

int main(int argc, char *argv[])
{

    size_t fsize = 4096;
    uint8_t *input = NULL;
    EntryInfo_t *entryinfos;
    if (NULL != getenv("DEBUG"))
        DEBUG = 1;
    if (argc < 2)
    {
        fprintf(stderr, "usage %s majordict [annodict] [JSON file]\n", argv[0]);
        return 127;
    }
    else
    {
        ssize_t rtn = readFile(argv[1], &input);
        if (rtn < 0)
            return (int)rtn;
        /*
        for (size_t i=0;i<rtn; i++) {
            printf("%02hhx%s", input[i], (i+1)%16?" ":"\n");
        }
        */
        // printf("read %lx(=%lu) bytes\n", rtn, rtn);
        fsize = (size_t)rtn;
    }
    ParseInfo_t parseInfo = {.buf = input, .ptr = input, .datalen = fsize, .offset = 0};
    parseDict(&parseInfo, &entryinfos);

    if (argc > 3)
    {
        // annodict + bejpayload
        uint8_t *annotation_input;
        EntryInfo_t *annotation_infos;
        ssize_t rtn = readFile(argv[2], &annotation_input);
        if (rtn < 0)
            return (int)rtn;
        ParseInfo_t parse_annotation = {.buf = annotation_input, .ptr = annotation_input, .datalen = (size_t)rtn, .offset = 0};
        // printf("read annotation %lx(=%lu) bytes\n", rtn, rtn);
        parseDict(&parse_annotation, &annotation_infos);

        uint8_t *json_input;
        rtn = readFile(argv[3], &json_input);
        cJSON *cjson_input = cJSON_Parse((const char *)json_input);
        BejTuple_t *bej_tuple_list = (BejTuple_t *)malloc(sizeof(BejTuple_t) * cJSON_GetArraySize(cjson_input));

        encodeJsonToBinary(bej_tuple_list, cjson_input, entryinfos, annotation_infos);
    }

    return 0;
}

#endif
