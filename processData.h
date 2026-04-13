#ifndef PROCESSDATA_H
#define PROCESSDATA_H

#include  "cJSON.h"

typedef struct processData
{
    char *nomeArquivo;
    cJSON *json;
}ARGSCARREGARJSON;

void *carregarJson(void *carregarJson);

#endif