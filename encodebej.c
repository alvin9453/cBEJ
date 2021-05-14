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

void pack_json_to_sf(BejTuple_t *bej_tuple_list, int* index, const cJSON *json, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    // parse cjson data to bej_tuple
    // but still not fill the length

    BejTuple_t bej_tuple;
    BejTupleS_t bejS;
    BejTupleF_t bejF;
    printf(" -- %s\n", json->string);

    const cJSON *obj = NULL;

    switch (json->type)
    {
        case cJSON_Object:
            printf(" cJSON Object \n");
            pack_json_to_sf(bej_tuple_list, index, json->child, major_dict, annotation_dict);
            break;
        case cJSON_Array:
            printf(" cJSON ARRAY \n");
            cJSON_ArrayForEach(obj, json->child)
            {
                pack_json_to_sf(bej_tuple_list, index, obj, major_dict, annotation_dict);
            }
            break;
        case cJSON_Number:
        case cJSON_True:
        case cJSON_False:
        case cJSON_String:
        default:
            // printf(" -- json->type = %d\n", json->type);
            break;
    }

    // int16_t major_dict_child_index = find_name_from_dictionary(json->string, major_dict);
    // if (major_dict_child_index > -1)
    // {
    //     // printf(" - [DEBUG] find \"%s\" in dictionary , seq number = %u\n", major_dict->ChildInfo[major_dict_child_index]->Name, major_dict->ChildInfo[major_dict_child_index]->SequenceNumber);
    //     bejS.seq = major_dict->ChildInfo[major_dict_child_index]->SequenceNumber;
    //     bejS.name = major_dict->ChildInfo[major_dict_child_index]->Name;
    //     bejS.annot_flag = 0;
        
        
    //     // BejTupleF_t
    //     // printf(" - [DEBUG] bejtype = %s\n", getBejtypeName(major_dict->ChildInfo[major_dict_child_index]->bejtype));
    //     uint8_t bejtype = major_dict->ChildInfo[major_dict_child_index]->bejtype;

    //     bejF.bejtype = major_dict->ChildInfo[major_dict_child_index]->bejtype;
    // }

    //     // Set annotation flag
    // int16_t anno_dict_child_index = find_name_from_dictionary(json->string, annotation_dict);
    // if (anno_dict_child_index > -1)
    // {
    //     // printf(" - [DEBUG] find \"%s\" is annotation. \n", annotation_dict->ChildInfo[anno_dict_child_index]->Name);
    //     bejS.annot_flag = 1;
    // }

    // if (major_dict_child_index == -1 && anno_dict_child_index == -1)
    // {
    //     // printf(" - !!!! [DEBUG] ERROR, cannot find \"%s\" property in dictionary.\n", json->string);
    //     return;
    // }

    // bej_tuple.bejS = bejS;
    // bej_tuple.bejF = bejF;

    // bej_tuple_list[*index] = bej_tuple;

    // (*index)++;

    return;
}

void encodeJsonToBinary(BejTuple_t *bej_tuple_list, const char *json_input, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    cJSON *cjson_input = cJSON_Parse(json_input);

    int i = 0;

    const cJSON *obj = NULL;
    // pack cjson to sfv
    cJSON_ArrayForEach(obj, cjson_input)
    {
        if(i < 64)
        {
            pack_json_to_sf(bej_tuple_list, &i, obj, major_dict, annotation_dict);
        }
        else
        {
            printf("Error");
        }
    }

    // Traverse the Linked list and calculate the length

    // Show BEJ encoded result
    // printBEJ(bej_tuple_node);
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
        // ParseInfo_t parsejsoninfo = {.buf = json_input, .ptr = json_input, .datalen = (size_t)rtn, .offset = 0};
        BejTuple_t *bej_tuple_list = (BejTuple_t *)malloc(sizeof(BejTuple_t) * 64);
        encodeJsonToBinary(bej_tuple_list, (const char *) json_input, entryinfos, annotation_infos);
        free(bej_tuple_list);
    }

    return 0;
}

#endif
