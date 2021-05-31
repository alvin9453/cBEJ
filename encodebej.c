#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encodebej.h"

extern int DEBUG;
#define ENCODE_TEST 1

#ifdef ENCODE_TEST

#define ENCODEBEJ_OUTPUT_BINARY_FILE "bejencode_result.bin"
#define MAX_PROPERTY_NAME_LENGTH 512

#define PRINT_LAYER(layer) \
   for(uint8_t i=0; i<(layer); i++) printf("    ")

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
        else if (dict->ChildInfo[i]->ChildCount > 0)
        {
            found_entry = find_entry_from_dictionary(name, dict->ChildInfo[i]);
            if (found_entry != NULL)
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
    void *bejV;
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
            bejS.seq = find_major_entry->SequenceNumber;
            bejS.name = find_major_entry->Name;
            bejS.annot_flag = 0;

            // BejTupleF_t
            bejF.bejtype = find_major_entry->bejtype;
        }

        // Set annotation flag
        EntryInfo_t *find_annotation_entry = find_entry_from_dictionary(json->string, annotation_dict);
        if (find_annotation_entry != NULL)
        {
            bejS.seq = find_annotation_entry->SequenceNumber;
            bejS.name = find_annotation_entry->Name;
            bejS.annot_flag = 1;

            bejF.bejtype = find_annotation_entry->bejtype;
        }

        if (find_major_entry == NULL && find_annotation_entry == NULL)
        {
            printf(" - !!!! [DEBUG] ERROR, cannot find \"%s\" property in dictionary.\n", json->string);
            return NULL;
        }
    }

    // BejV
    switch (bejF.bejtype)
    {
    case bejSet:
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
    case bejArray:
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
    case bejString:
        bejV = malloc(strlen(json->valuestring) + 1);
        memcpy(bejV, (char *)json->valuestring, strlen(json->valuestring) + 1);
        break;
    case bejEnum:
        venum = malloc(sizeof(bejEnum_t));
        if(json->type == cJSON_String)
        {
            venum->name = malloc(strlen(json->valuestring) + 1);
            memcpy((char *)venum->name, (char *)json->valuestring, strlen(json->valuestring) + 1);
            EntryInfo_t *find_major_entry = find_entry_from_dictionary(json->valuestring, major_dict);
            venum->nnint = find_major_entry->SequenceNumber;
            bejV = venum;
        }
        else
        {
            venum->name = NULL;
            venum->nnint = 0;
            bejV = venum;
        }
        break;
    case bejInteger:
        vinteger = malloc(sizeof(bejInteger_t));
        vinteger->value = (bejint_t)json->valuedouble;
        bejV = vinteger;
        break;
    case bejBoolean:
        vbool = malloc(sizeof(bejBoolean_t));
        if(json->type == cJSON_True)
            vbool->value = 0xff;
        else
            vbool->value = 0x00;
        bejV = vbool;
        break;
    case bejNull:
        bejV = NULL;
        break;
    default:
        break;
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
    nnint_t bejv_length = 0;
    nnint_t another_nnint_count = 2; // BejS and BejL

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
                bejL += set_tuple_length(bejtuple) + sizeof(nnint_t) + 1;
            }
            bejv_length = bejL;
            tuple->bejL = bejL;
        }
        break;
    case bejArray:
        varray = (bejArray_t *)tuple->bejV;
        count = varray->count;
        for (nnint_t i = 0; i < count; i++)
        {
            bejtuple = &varray->tuples[i];
            bejL += set_tuple_length(bejtuple);
        }
        bejv_length = bejL;
        tuple->bejL = bejL;
        break;
    case bejString:
        vstring = (bejString_t *)tuple->bejV;
        bejL = strlen((const char *)vstring) + 1;
        bejv_length = bejL;
        tuple->bejL = bejL;
        break;
    case bejInteger:
        vinteger = (bejInteger_t *)tuple->bejV;
        bejL = sizeof(vinteger->value);
        tuple->bejL = bejL;
        bejv_length = bejL;
        break;
    case bejEnum:
        venum = (bejEnum_t *)tuple->bejV;
        bejL = sizeof(venum->nnint) + 1; // one nnint byte;
        tuple->bejL = bejL;
        bejv_length = bejL;
        break;
    case bejBoolean:
        vbool = (bejBoolean_t *)tuple->bejV;
        bejL = sizeof(vbool->value);
        tuple->bejL = bejL;
        bejv_length = bejL;
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
    if (strncmp(bejS->name, "root_tuple", strnlen(bejS->name, MAX_PROPERTY_NAME_LENGTH)) == 0)
    {
        // root tuple only need to know the length of BejV
        return bejv_length;
    }
    else
    {
        total_bej_tuple_slvf_length = sizeof(bejS->seq) + sizeof(BejTupleF_t) + sizeof(bejL) + bejv_length + another_nnint_count;
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
        printf(" - [%d] %s , \"bejEnum\", L = %d, value = (%d)%s\n", bejS->seq, bejS->name, *bejL, venum->nnint, venum->name);
        break;
    case bejBoolean:
        vbool = (bejBoolean_t *)tuple->bejV;
        printf(" - [%d] %s , \"bejBoolean\", L = %d, value = %s\n", bejS->seq, bejS->name, *bejL, vbool->value == 0x00 ? "false" : "true");
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
        fwrite(bejL, sizeof(BejTupleL_t),1, output_file);

        switch (bejF->bejtype)
        {
        case bejSet:
            vset = (bejSet_t *)tuple->bejV;
            count = vset->count;

            fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
            fwrite(&count, sizeof(nnint_t),1, output_file);
            break;
        case bejArray:
            varray = (bejArray_t *)tuple->bejV;
            count = varray->count;

            fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
            fwrite(&count, sizeof(nnint_t), 1, output_file);
            break;
        case bejEnum:
            venum = (bejEnum_t *)tuple->bejV;
            fwrite(&nnint_size, sizeof(uint8_t), 1, output_file);
            fwrite(&venum->nnint, sizeof(nnint_t), 1, output_file);
            break;
        default:
            fwrite(tuple->bejV, *bejL, 1, output_file);
            break;
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
        // printf(" [DEBUG] seq = %d, name = %s, annotation_flag = %d, type = %s\n", tuple->bejS.seq, tuple->bejS.name, tuple->bejS.annot_flag, getBejtypeName(tuple->bejF.bejtype));
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

    if(DEBUG){
        showTuple(root_bej_tuple, 0);
    }

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
        }

        BejTuple_t *bej_tuple_list = encodeJsonToBinary(cjson_input, entryinfos, annotation_infos);

        // showTuple(bej_tuple_list, 0);
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

    return 0;
}

#endif
