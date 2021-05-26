#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encodebej.h"

extern int DEBUG;
#define ENCODE_TEST 1

#ifdef ENCODE_TEST

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

BejTuple_t *pack_json_to_sfv(const cJSON *json, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
{
    // parse cjson data to bej_tuple
    // but still not fill the length

    BejTupleS_t bejS;
    BejTupleF_t bejF;
    void *bejV;
    bejSet_t *vset;
    bejArray_t *varray;
    BejTuple_t *packed_result;
    int index = 0;
    printf(" -- %s\n", json->string == NULL ? "<SET>" : json->string) ;

    const cJSON *obj = NULL;

    if (json->type == cJSON_Object)
    {
        vset = malloc(sizeof(bejSet_t));
        vset->count = cJSON_GetArraySize(json);
        if (cJSON_GetArraySize(json) != 0)
        {
            vset->tuples = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
            BejTuple_t *vset_tuples_p = vset->tuples;

            cJSON_ArrayForEach(obj, json)
            {
                packed_result = pack_json_to_sfv(obj, major_dict, annotation_dict);
                if (packed_result != NULL)
                {
                    // printf(" [DEBUG] seq = %d, name = %s, annotation_flag = %d, type = %s\n", packed_result->bejS.seq, packed_result->bejS.name, packed_result->bejS.annot_flag, getBejtypeName(packed_result->bejF.bejtype));
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
    }
    else if (json->type == cJSON_Array)
    {
        varray = malloc(sizeof(bejArray_t));
        varray->tuples = malloc(cJSON_GetArraySize(json) * sizeof(BejTuple_t));
        varray->count = cJSON_GetArraySize(json);
        BejTuple_t *varray_tuples_p = varray->tuples;
        cJSON_ArrayForEach(obj, json)
        {
            // printf(" ---- else obj = %s, obj->child->type = %d\n", obj->child->string, obj->child->type);
            packed_result = pack_json_to_sfv(obj, major_dict, annotation_dict);
            memcpy(&varray->tuples[index++], packed_result, sizeof(BejTuple_t));
        }
        varray->tuples = varray_tuples_p;
        bejV = varray;
    }
    else
    {
        if (json->type == cJSON_String)
        {
            const char *json_string = json->valuestring;
            bejV = malloc(strlen(json_string));
            memcpy(bejV, (char *)json_string, strlen(json_string) + 1);
        }
        else if (json->type == cJSON_Number)
        {
            bejInteger_t *bej_integer = malloc(sizeof(bejInteger_t));
            bej_integer->value = (bejint_t)json->valuedouble;
            bejV = bej_integer;
        }
        else if (cJSON_IsTrue(json))
        {
            bejBoolean_t *bej_bool = malloc(sizeof(bejBoolean_t));
            bej_bool->value = 1;
            bejV = bej_bool;
        }
        else if (cJSON_IsFalse(json))
        {
            bejBoolean_t *bej_bool = malloc(sizeof(bejBoolean_t));
            bej_bool->value = 0;
            bejV = bej_bool;
        }
        else if (cJSON_IsNull)
        {
            bejV = NULL;
        }
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
            bejS.name = find_major_entry->Name; // One bug is here
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

void showTuple(BejTuple_t *tuple)
{
    BejTupleS_t *bejS = &tuple->bejS;
    BejTupleF_t *bejF = &tuple->bejF;
    bejSet_t *vset;
    bejArray_t *varray;
    bejInteger_t *vinteger;
    bejString_t *vstring;
    nnint_t count;
    BejTuple_t *bejtuple;
    // printf(" [showTuple] seq = %d, name = %s, annotation_flag = %d, type = %s\n", bejS->seq, bejS->name, bejS->annot_flag, getBejtypeName(bejF->bejtype));
    if (bejF->bejtype == bejSet)
    {
        printf(" - [%d] \"bejSet\" name = %s, annotation_flag = %d\n", bejS->seq, bejS->name == "" ? "<SET>" : bejS->name, bejS->annot_flag);
        vset = (bejSet_t *)tuple->bejV;
        if (vset != NULL)
        {
            count = vset->count;
            for (nnint_t i = 0; i < count; i++)
            {
                bejtuple = &vset->tuples[i];
                // printf(" - [%d] \"bejSet\" name = %s, annotation_flag = %d\n", bejtuple->bejS.seq, bejtuple->bejS.name, bejtuple->bejS.annot_flag);
                showTuple(bejtuple);
            }
        }
    }
    else if (bejF->bejtype == bejArray)
    {
        printf(" - [%d] \"bejArray\" name = %s, annotation_flag = %d\n", bejS->seq, bejS->name, bejS->annot_flag);
        varray = (bejArray_t *)tuple->bejV;
        count = varray->count;
        for (nnint_t i = 0; i < count; i++)
        {
            bejtuple = &varray->tuples[i];
            showTuple(bejtuple);
        }

    }
    else if (bejF->bejtype == bejString)
    {
        vstring = (bejString_t *)tuple->bejV;
        printf(" - [%d] \"bejString\" name = %s, value = %s\n", bejS->seq, bejS->name, (const char *)vstring);
    }
    else if (bejF->bejtype == bejInteger)
    {
        vinteger = (bejInteger_t *)tuple->bejV;
        printf(" - [%d] \"bejInteger\" name = %s, value = %lld\n", bejS->seq, bejS->name, vinteger->value);
    }
    else if (bejF->bejtype == bejEnum)
    {
        printf(" - [%d] \"bejEnum\" name = %s, value = %s\n", bejS->seq, bejS->name, (const char *)tuple->bejV);
    }
    /*
        TODO : the other BEJ type
    */
}

void encodeJsonToBinary(BejTuple_t *bej_tuple_list, cJSON *json_input, EntryInfo_t *major_dict, EntryInfo_t *annotation_dict)
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
        // printf(" [DEBUG] seq = %d, name = %s, annotation_flag = %d, type = %s\n", tuple->bejS.seq, tuple->bejS.name, tuple->bejS.annot_flag, getBejtypeName(tuple->bejF.bejtype));
        memcpy(root_bej_V_init_ptr, tuple, sizeof(BejTuple_t));

        root_bej_V_init_ptr++;
    }
    root_bej_set->tuples = malloc(sizeof(BejTuple_t));
    root_bej_set->tuples = root_bej_V;
    root_bej_set->count = cJSON_GetArraySize(json_input);

    root_bej_tuple->bejV = root_bej_set;

    printf("------------------------------------------\n");
    showTuple(root_bej_tuple);

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
