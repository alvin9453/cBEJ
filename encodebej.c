#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encodebej.h"

extern int DEBUG;
#define ENCODE_TEST 1

#ifdef ENCODE_TEST

int16_t find_name_from_dictionary(char *name, EntryInfo_t *dict)
{
    int16_t index = -1;
    for (uint16_t i = 0; i < dict->ChildCount; i++)
    {
        if (strncmp(dict->ChildInfo[i]->Name, name, sizeof(dict->ChildInfo[i]->Name)) == 0)
        {
            index = i;
            break;
        }
    }
    return index;
}

BejTuple_t *pack_json_to_sfv(const cJSON *json, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    // parse cjson data to bej_tuple
    // but still not fill the length

    BejTupleS_t bejS;
    BejTupleF_t bejF;
    void **bej_V;
    printf(" -- %s\n", json->string);

    const cJSON *obj = NULL;

    if (json->type == cJSON_Object)
    {
        bej_V = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
        int index = 0;
        cJSON_ArrayForEach(obj, json)
        {
            bej_V[index++] = pack_json_to_sfv(obj, major_dict, annotation_dict);
        }
    }
    else if (json->type == cJSON_Array)
    {
        int index = 0;
        cJSON_ArrayForEach(obj, json)
        {
            bej_V = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
            bej_V[index++] = pack_json_to_sfv(obj->child, major_dict, annotation_dict);
        }
    }
    else
    {
        if (json->type == cJSON_String)
        {
            const char *json_string;
            json_string = json->valuestring;
            bej_V = malloc(1 * sizeof(json_string));
            memcpy(bej_V, json_string, sizeof(json_string));
        }
        else if (json->type == cJSON_Number)
        {
            double *json_valuedouble_p;
            double json_valuedouble = json->valuedouble;
            json_valuedouble_p = &(json_valuedouble);
            bej_V = malloc(1 * sizeof(double));
            memcpy(bej_V, json_valuedouble_p, sizeof(json_valuedouble_p));
        }
        else if (cJSON_IsTrue(json))
        {
            int true_value = 0;
            int *true_value_p;
            true_value_p = &true_value;
            bej_V = malloc(sizeof(int));
            memcpy(bej_V, true_value_p, sizeof(true_value_p));
        }
        else if (cJSON_IsFalse(json))
        {
            int false_value = 0;
            int *false_value_p;
            false_value_p = &false_value;
            bej_V = malloc(sizeof(int));
            memcpy(bej_V, false_value_p, sizeof(false_value_p));
        }
    }

    int16_t major_dict_child_index = find_name_from_dictionary(json->string, major_dict);
    if (major_dict_child_index > -1)
    {
        // printf(" - [DEBUG] find \"%s\" in dictionary , seq number = %u\n", major_dict->ChildInfo[major_dict_child_index]->Name, major_dict->ChildInfo[major_dict_child_index]->SequenceNumber);
        bejS.seq = major_dict->ChildInfo[major_dict_child_index]->SequenceNumber;
        bejS.name = major_dict->ChildInfo[major_dict_child_index]->Name;
        bejS.annot_flag = 0;

        // BejTupleF_t
        // printf(" - [DEBUG] bejtype = %s\n", getBejtypeName(major_dict->ChildInfo[major_dict_child_index]->bejtype));
        uint8_t bejtype = major_dict->ChildInfo[major_dict_child_index]->bejtype;

        bejF.bejtype = major_dict->ChildInfo[major_dict_child_index]->bejtype;
    }

    // Set annotation flag
    int16_t anno_dict_child_index = find_name_from_dictionary(json->string, annotation_dict);
    if (anno_dict_child_index > -1)
    {
        printf(" - [DEBUG] find \"%s\" is annotation. \n", annotation_dict->ChildInfo[anno_dict_child_index]->Name);
        bejS.seq = annotation_dict->ChildInfo[anno_dict_child_index]->SequenceNumber;
        bejS.annot_flag = 1;
    }

    if (major_dict_child_index == -1 && anno_dict_child_index == -1)
    {
        printf(" - !!!! [DEBUG] ERROR, cannot find \"%s\" property in dictionary.\n", json->string);
        return NULL;
    }else{
        BejTuple_t *bej_tuple = malloc(sizeof(BejTuple_t));
        bej_tuple->bejS = bejS;
        bej_tuple->bejF = bejF;
        bej_tuple->bejV = bej_V;

        return bej_tuple;
    }
}

void showTuple(BejTuple_t **bej_tuple, int count)
{
    for(int i = 0; i <= count; i++)
    {
        printf(" [%d] name = %s, type = %s \n", bej_tuple[i]->bejS.seq, bej_tuple[i]->bejS.name, getBejtypeName(bej_tuple[i]->bejF.bejtype));
    }
}

void encodeJsonToBinary(BejTuple_t *bej_tuple_list, cJSON *json_input, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    const cJSON *obj = NULL;

    // Create root tuple
    BejTupleS_t bejS = {.seq = 0, .annot_flag = 0, .name = ""};
    BejTupleF_t bejF = {.bejtype = bejSet};
    BejTuple_t root_bej_tuple = {.bejS = bejS, .bejF = bejF};
    BejTuple_t **root_bej_V = malloc(cJSON_GetArraySize(json_input) * sizeof(BejTuple_t));

    int index = 0;
    cJSON_ArrayForEach(obj, json_input)
    {        
        root_bej_V[index++] = pack_json_to_sfv(obj, major_dict, annotation_dict);
    }
    printf("------------------------------------------\n");
    printf(" index = %d\n", index);
    showTuple(root_bej_V, index-1);

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
        fprintf(stderr, "usage %s majordict [annodict] [bejpayload]\n", argv[0]);
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
