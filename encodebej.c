#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encodebej.h"

extern int DEBUG;
#define ENCODE_TEST 1

#ifdef ENCODE_TEST

#define ENCODEBEJ_OUTPUT_BINARY_FILE "bejencode_result.bin"
#define MAX_PROPERTY_NAME_LENGTH 512

#define ERROR_INVALID_JSON_INPUT 0x01
#define ERROR_ENTRY_NOT_FOUND_IN_DICTIONARY 0x02
#define ERROR_JSON_TYPE_NOT_MATCH_BEJ_TYPE 0x03


#define PRINT_LAYER(layer) \
   for(uint8_t i=0; i<(layer); i++) printf("    ")

static uint8_t errno = 0x00;

EntryInfo_t *find_entry_from_dictionary(char *name, EntryInfo_t *dict)
{
    if (name == NULL)
        return NULL;

    EntryInfo_t *found_entry = NULL;
    for (uint16_t i = 0; i < dict->ChildCount; i++)
    {
        if (strncmp(name, dict->ChildInfo[i]->Name, strnlen(name, MAX_PROPERTY_NAME_LENGTH) + 1) == 0)
        {
            found_entry = dict->ChildInfo[i];
            break;
        }
    }
    return found_entry;
}

// Pack cJSON data into BEJ Tuple (SFV , not L).
// Note that all nnint in this function is NOT the DMTF-defined format, the first bytes didn't present the length
BejTuple_t *pack_json_to_sfv(const cJSON *json, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    // parse cjson data to bej_tuple, but still not fill the length

    BejTupleS_t bejS;
    BejTupleF_t bejF;
    void *bejV = NULL;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *vinteger;
    bejBoolean_t *vbool;
    bejEnum_t *venum;
    BejTuple_t *varray_tuples_p;
    BejTuple_t *vset_tuples_p;
    BejTuple_t *packed_result;
    int index = 0;
    const cJSON *obj = NULL;
    EntryInfo_t *find_major_entry = major_dict;
    EntryInfo_t *find_annotation_entry = annotation_dict;

    if(json->string == NULL) 
    {
        bejS.seq = 0;
        bejS.name = "";
        bejS.annot_flag = 0;

        bejF.bejtype = major_dict->ChildInfo[0]->bejtype;
        find_major_entry = major_dict->ChildInfo[0];
    }
    else
    {
        if ((find_major_entry = find_entry_from_dictionary(json->string, major_dict)) != NULL)
        {
            bejS.seq = find_major_entry->SequenceNumber;
            bejS.name = find_major_entry->Name;
            bejS.annot_flag = 0;

            bejF.bejtype = find_major_entry->bejtype;
        }
        else if ((find_annotation_entry = find_entry_from_dictionary(json->string, annotation_dict)) != NULL)
        {
            bejS.seq = find_annotation_entry->SequenceNumber;
            bejS.name = find_annotation_entry->Name;
            bejS.annot_flag = 1;

            bejF.bejtype = find_annotation_entry->bejtype;
            
            find_major_entry = major_dict;
        }else
        {
            printf(" - !!!! [DEBUG] ERROR, cannot find \"%s\" property in dictionary.\n", json->string);
            errno = ERROR_ENTRY_NOT_FOUND_IN_DICTIONARY;
            return NULL;
        }
    }

    if(cJSON_IsNull(json))
    {
        bejV = NULL;
    }
    else
    {
        // BejV
        nnint_t childseq = 0;
        switch (bejF.bejtype)
        {
        case bejSet:
            if(cJSON_IsObject(json)) {
                vset = malloc(sizeof(bejSet_t));
                vset->count = cJSON_GetArraySize(json);
                if (cJSON_GetArraySize(json) != 0)
                {
                    vset->tuples = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
                    vset_tuples_p = vset->tuples;

                    cJSON_ArrayForEach(obj, json)
                    {
                        packed_result = pack_json_to_sfv(obj, find_major_entry, find_annotation_entry);
                        if (packed_result == NULL)
                        {
                            printf(" - !!!! [DEBUG] ERROR while pack json into tuple in Json Object , current Json propert is \"%s\" \n", json->string);
                            return NULL;
                        }
                        memcpy(&vset->tuples[index++], packed_result, sizeof(*packed_result));
                    }
                    vset->tuples = vset_tuples_p;
                }
                else
                {
                    vset->tuples = NULL;
                }
                bejV = vset;
            }
            else {
                printf(" - !!!! [DEBUG] ERROR bejSet should input Json object, current Json propert is \"%s\" \n", json->string?json->string:"");
                return NULL;
            }
            break;
        case bejArray:
            if(cJSON_IsArray(json)) {
                varray = malloc(sizeof(bejArray_t));
                varray->tuples = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
                varray->count = cJSON_GetArraySize(json);
                varray_tuples_p = varray->tuples;
                cJSON_ArrayForEach(obj, json)
                {
                    packed_result = pack_json_to_sfv(obj, find_major_entry, find_annotation_entry);
                    if (packed_result == NULL)
                    {
                        printf(" - !!!! [DEBUG] ERROR while pack json into tuple in Json Array , current Json propert is \"%s\" \n", json->string);
                        return NULL;
                    }
                    packed_result->bejS.seq = childseq++;
                    memcpy(&varray->tuples[index++], packed_result, sizeof(BejTuple_t));
                }
                varray->tuples = varray_tuples_p;
                bejV = varray;
            }
            else{
                printf(" - !!!! [DEBUG] ERROR bejArray should input Json array, current Json propert is \"%s\" \n", json->string?json->string:"");
                errno = ERROR_JSON_TYPE_NOT_MATCH_BEJ_TYPE;
                return NULL;
            }
            break;
        case bejString:
            if(cJSON_IsString(json)) {
                bejV = malloc(strlen(json->valuestring) + 1);
                memcpy(bejV, (char *)json->valuestring, strlen(json->valuestring) + 1);
            }
            else {
                printf(" - !!!! [DEBUG] ERROR bejString should input Json array, current Json propert is \"%s\" \n", json->string?json->string:"");
                errno = ERROR_JSON_TYPE_NOT_MATCH_BEJ_TYPE;
                return NULL;
            }
            break;
        case bejEnum:
            if(cJSON_IsString(json))
            {
                venum = malloc(sizeof(bejEnum_t));
                venum->name = malloc(strlen(json->valuestring) + 1);
                memcpy((char *)venum->name, (char *)json->valuestring, strlen(json->valuestring) + 1);
                EntryInfo_t *find_enum_entry = find_entry_from_dictionary(json->valuestring, find_major_entry);
                if (find_enum_entry == NULL)
                {
                    printf(" - !!!! [DEBUG] ERROR find Enum member , current Json propert is \"%s\" \n", json->string);
                    errno = ERROR_ENTRY_NOT_FOUND_IN_DICTIONARY;
                    return NULL;
                }
                venum->nnint = find_enum_entry->SequenceNumber;
                bejV = venum;
            }
            else
            {
                printf(" - !!!! [DEBUG] ERROR bejEnum should input Json array, current Json propert is \"%s\" \n", json->string?json->string:"");
                errno = ERROR_JSON_TYPE_NOT_MATCH_BEJ_TYPE;
                return NULL;
            }
            break;
        case bejInteger:
            if(cJSON_IsNumber(json)){
                vinteger = malloc(sizeof(bejInteger_t));
                vinteger->value = (bejint_t)json->valuedouble;
                bejV = vinteger;
            }
            else {
                printf(" - !!!! [DEBUG] ERROR bejInteger should input Json number, current Json propert is \"%s\" \n", json->string?json->string:"");
                errno = ERROR_JSON_TYPE_NOT_MATCH_BEJ_TYPE;
                return NULL;
            }
            break;
        case bejBoolean:
            if(cJSON_IsBool(json)){
                vbool = malloc(sizeof(bejBoolean_t));
                if(json->type == cJSON_True)
                    vbool->value = 0xff;
                else
                    vbool->value = 0x00;
                bejV = vbool;
            }
            else {
                printf(" - !!!! [DEBUG] ERROR bejBoolean should input Json bool, current Json propert is \"%s\" \n", json->string?json->string:"");
                errno = ERROR_JSON_TYPE_NOT_MATCH_BEJ_TYPE;
                return NULL;
            }
            break;
        case bejNull:
            bejV = NULL;
            break;
        default:
            break;
        }
    }
    BejTuple_t *bej_tuple = malloc(sizeof(BejTuple_t));
    bej_tuple->bejS = bejS;
    bej_tuple->bejF = bejF;
    bej_tuple->bejV = bejV;

    // printf(" [DEBUG] seq = %d, name = %s, annotation_flag = %d, type = %s\n", bejS.seq, bejS.name, bejS.annot_flag, getBejtypeName(bejF.bejtype));

    return bej_tuple;

}

// Calculate tye BejL based on the pack_json_to_sfv result
nnint_t set_tuple_length(BejTuple_t *tuple)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    BejTupleL_t bejL = 0;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *vinteger;
    bejString_t *vstring;
    bejBoolean_t *vbool;
    bejEnum_t *venum;
    nnint_t count = 0;
    BejTuple_t *bejtuple;
    nnint_t total_bej_tuple_slvf_length = 0;
    nnint_t another_nnint_count = 2; // BejS and BejL nnint bytes

    switch (bejF->bejtype)
    {
    case bejSet:
        vset = (bejSet_t *)tuple->bejV;
        if (vset != NULL)
        {
            count = vset->count;
            bejL +=  sizeof(nnint_t) + 1; //  [ sizeof(nnint_t) + 1 ] is for set member count
            for (nnint_t i = 0; i < count; i++)
            {
                bejtuple = &vset->tuples[i];
                bejL += set_tuple_length(bejtuple);
            }
        }
        break;
    case bejArray:
        varray = (bejArray_t *)tuple->bejV;
        if(varray != NULL)
        {
            count = varray->count;
            bejL +=  sizeof(nnint_t) + 1; //  [ sizeof(nnint_t) + 1 ] is for set member count
            for (nnint_t i = 0; i < count; i++)
            {
                bejtuple = &varray->tuples[i];
                bejL += set_tuple_length(bejtuple);
            }
        }
        break;
    case bejString:
        vstring = (bejString_t *)tuple->bejV;
        if(vstring != NULL)
            bejL = strlen((const char *)vstring) + 1;
        break;
    case bejInteger:
        vinteger = (bejInteger_t *)tuple->bejV;
        if(vinteger != NULL)
            bejL = sizeof(vinteger->value);
        break;
    case bejEnum:
        venum = (bejEnum_t *)tuple->bejV;
        if (venum != NULL)
            bejL = sizeof(venum->nnint) + 1; // one nnint byte;
        break;
    case bejBoolean:
        vbool = (bejBoolean_t *)tuple->bejV;
        if(vbool != NULL)
            bejL = sizeof(vbool->value);
        break;
    case bejNull:
        bejL = 0;
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
    tuple->bejL = bejL;
    if (strncmp(bejS->name, "root_tuple", strnlen(bejS->name, MAX_PROPERTY_NAME_LENGTH)) == 0)
    {
        // root tuple only need to know the length of BejV
        return bejL;
    }
    else
    {
        total_bej_tuple_slvf_length = sizeof(bejS->seq) + sizeof(BejTupleF_t) + sizeof(BejTupleL_t) + bejL + another_nnint_count;
        return total_bej_tuple_slvf_length;
    }
}

// For Debug usage : Print all tuples data
void showTuple(BejTuple_t *tuple, int layer)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    BejTupleL_t *bejL = &tuple->bejL;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *vinteger;
    bejString_t *vstring;
    bejBoolean_t *vbool;
    bejEnum_t *venum;
    nnint_t count;
    BejTuple_t *bejtuple;

    PRINT_LAYER(layer);
    switch (bejF->bejtype)
    {
    case bejSet:
        printf(" - [%d] %s , \"bejSet\", L = %d \n", bejS->seq, bejS->name == "" ? "<SET>" : bejS->name, *bejL);
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
        printf(" - [%d] %s , \"bejArray\", L = %d\n", bejS->seq, bejS->name, *bejL);
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
        printf(" - [%d] %s , \"bejString\", L = %d, value = %s\n", bejS->seq, bejS->name, *bejL, strlen((const char *)vstring) > 1 ? (const char *)vstring : "\"\"");
        break;
    case bejInteger:
        vinteger = (bejInteger_t *)tuple->bejV;
        printf(" - [%d] %s , \"bejInteger\", L = %d, value = %lld\n", bejS->seq, bejS->name, *bejL, vinteger->value);
        break;
    case bejEnum:
        venum = (bejEnum_t *)tuple->bejV;
        printf(" - [%d] %s , \"bejEnum\", L = %d, ", bejS->seq, bejS->name, *bejL);
        if(venum != NULL)
            printf(" value = (%d)%s\n", venum->nnint, venum->name);
        else
            printf(" value = NULL\n");
        break;
    case bejBoolean:
        vbool = (bejBoolean_t *)tuple->bejV;
        printf(" - [%d] %s , \"bejBoolean\", L = %d, value = %s\n", bejS->seq, bejS->name, *bejL, vbool->value == 0x00 ? "false" : "true");
        break;
    case bejNull:
        printf(" - [%d] %s , \"bejNull\", L = %d\n value = null", bejS->seq, bejS->name, *bejL);
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

void outputBejTupleToFile(BejTuple_t *tuple, FILE *output_file)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    BejTupleL_t *bejL = &tuple->bejL;

    bejSet_t *vset;
    bejArray_t *varray;
    bejEnum_t *venum;
    nnint_t count = 0;
    uint8_t nnint_size = sizeof(nnint_t);
    nnint_t annotation_flag = bejS->annot_flag;
    nnint_t seq = bejS->seq << 1 | !!annotation_flag;

    if (output_file != NULL)
    {
        // Write BejS
        fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
        fwrite(&seq, sizeof(nnint_t), 1, output_file);

        // Write BejF
        fwrite(bejF, sizeof(BejTupleF_t), 1, output_file);

        // Write BejL
        fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
        fwrite(bejL, sizeof(BejTupleL_t), 1, output_file);

        if(DEBUG)
        {
            printf(" S = %02x %06x , ", nnint_size, seq);
            printf(" F = %02x (%s) , ", bejF->bejtype, getBejtypeName(bejF->bejtype));
            printf(" L = %02x %06x , ", nnint_size, *bejL);
        }
        
        if(*bejL > 0)
        {
            switch (bejF->bejtype)
            {
            case bejSet:
                vset = (bejSet_t *)tuple->bejV;
                count = vset->count;

                fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
                fwrite(&count, sizeof(nnint_t),1, output_file);

                if(DEBUG)
                {
                    printf(" V(count) = %02x %06x\n", nnint_size, count);
                } 
                break;
            case bejArray:
                varray = (bejArray_t *)tuple->bejV;
                count = varray->count;

                fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
                fwrite(&count, sizeof(nnint_t), 1, output_file);
                if (DEBUG)
                {
                    printf(" V(count) = %02x %06x\n", nnint_size, count);
                }
                break;
            case bejEnum:
                venum = (bejEnum_t *)tuple->bejV;
                fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
                fwrite(&venum->nnint, sizeof(nnint_t), 1, output_file);

                if (DEBUG)
                {
                    printf(" V = %02x %06x\n", nnint_size, venum->nnint);
                }
                break;
            case bejNull:
                break;
            default:
                fwrite(tuple->bejV, *bejL, 1, output_file);
                if (DEBUG)
                {
                    printf(" V = %02x %06x\n", nnint_size, venum->nnint);
                }
                break;
            }
        }else{
            if(DEBUG)
            {
                printf(" V = NULL\n");
            }
        }
    }
}

void outputBejBasicToFile(FILE *output_file)
{
    BejBasic_t bej_basic = {.ver32 = 0xF1F0F000, .flags = 0, .schemaclass = 0}; // DSP0218_1.0.1
    fwrite(&bej_basic, sizeof(bej_basic), 1, output_file);
}

// Traverse all BEJ Tuples and output binary to files
void outputBejEncodeResult(BejTuple_t *tuple, FILE *output_file)
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
        outputBejTupleToFile(tuple, output_file);
        vset = (bejSet_t *)tuple->bejV;
        if (vset != NULL)
        {
            count = vset->count;
            for (nnint_t i = 0; i < count; i++)
            {
                bejtuple = &vset->tuples[i];
                outputBejEncodeResult(bejtuple, output_file);
            }
        }
        free(tuple->bejV);
        break;
    case bejArray:
        outputBejTupleToFile(tuple, output_file);
        varray = (bejArray_t *)tuple->bejV;
        count = varray->count;
        for (nnint_t i = 0; i < count; i++)
        {
            bejtuple = &varray->tuples[i];
            outputBejEncodeResult(bejtuple, output_file);
        }
        free(tuple->bejV);
        break;
    case bejString:
    case bejInteger:
    case bejEnum:
    case bejBoolean:
    case bejNull:
    default:
        outputBejTupleToFile(tuple, output_file);
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

BejTuple_t *encodeJsonToBinary(cJSON *json_input, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
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

        if(tuple == NULL)
        {
            printf(" [ERROR] failed to parse JSON into BEJ tuple \n");
            return NULL;
        }
        memcpy(root_bej_V_init_ptr, tuple, sizeof(BejTuple_t));

        root_bej_V_init_ptr++;
    }
    root_bej_set->tuples = malloc(sizeof(BejTuple_t));
    root_bej_set->tuples = root_bej_V;
    root_bej_set->count = cJSON_GetArraySize(json_input);
    root_bej_tuple->bejV = root_bej_set;  

    nnint_t bejv_length = 0;
    bejv_length = set_tuple_length(root_bej_tuple);
    root_bej_tuple->bejL = bejv_length;

    return root_bej_tuple;
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
        if(cJSON_IsInvalid(cjson_input))
        {
            printf(" - [ERROR] not a valid json \n");
            errno = ERROR_INVALID_JSON_INPUT;
        }
        else
        {
            BejTuple_t *bej_tuple_list = encodeJsonToBinary(cjson_input, entryinfos, annotation_infos);
            if (bej_tuple_list == NULL)
            {
                printf(" [ERROR] failed to parse JSON into BEJ tuple \n");
            }
            else
            {
                if (DEBUG)
                {
                    showTuple(bej_tuple_list, 0);
                }
                FILE *output_file = NULL;

                if ((output_file = fopen(ENCODEBEJ_OUTPUT_BINARY_FILE, "wb+")) != NULL)
                {
                    // Write BEJ basic headers
                    outputBejBasicToFile(output_file);

                    // Write all tuples
                    outputBejEncodeResult(bej_tuple_list, output_file);
                }
                fclose(output_file);
            }
        }
        if(errno != 0)
            printf(" [ERROR] Found there is an error, error no : %02x\n", errno);
    }

    return 0;
}

#endif
