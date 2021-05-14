#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "cJSON.h"

ssize_t readFile(char *filename, uint8_t **input)
{
    FILE *inputfile = NULL;
    size_t fsize = 0;
    inputfile = fopen(filename, "r");
    if (inputfile == NULL)
        perror("Error opening file");
    else
    {
        fseek(inputfile, 0, SEEK_END);
        fsize = (size_t)ftell(inputfile);
        rewind(inputfile);
        *input = (uint8_t *)malloc(sizeof(uint8_t) * fsize);
        // printf("input=%p\n", *input);
        if (NULL == *input)
        {
            perror("malloc failed");
            return -1;
        }
        if (fsize != fread(*input, sizeof(uint8_t), fsize, inputfile))
        {
            perror("fread failed");
            return -2;
        }
    }
    return fsize;
}

int main(int argc, char *argv[])
{
    uint8_t *json_input;
    readFile(argv[1], &json_input);
    // printf("%s\n", json_input);
    cJSON *json = cJSON_Parse(json_input);

    const cJSON *name = NULL;
    name = cJSON_GetObjectItemCaseSensitive(json, "Controllers");

    const cJSON *items;
    const cJSON *item;
    cJSON_ArrayForEach(item, name->child)
    {
        if(cJSON_IsObject(item)){
            printf("%s : %f \n", item->child->child->string, item->child->child->valuedouble);
        }
        // printf("%s : %s \n", item->string, item->valuestring);
    }


    // const cJSON *name = NULL;
    // printf("name = %s\n", name->child->valuestring);
    // const cJSON *items;
    // const cJSON *item;
    // // cJSON_ArrayForEach(item, json)
    // {
    //     if(cJSON_IsString(item))
    //         printf("%s : %s \n", item->string, item->valuestring);
    //     // printf("%s : %s \n", item->string, item->valuestring);
    // }
}