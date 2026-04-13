#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "processData.h"
#include "cJSON.h"

void *carregarJson(void *args)
{
    ARGSCARREGARJSON *parametros = (ARGSCARREGARJSON *)args;

    FILE *f = fopen(parametros->nomeArquivo, "r");

    if(!f)
    {
        printf("Erro ao abrir arquivo!\n");
        exit(EXIT_FAILURE);
    }

    fseek(f,0,SEEK_END);
    long tamanhoArquivo = ftell(f);
    fseek(f,0,SEEK_SET);

    char *buffer = (char *)malloc(tamanhoArquivo+1);
    
    fread(buffer, 1, tamanhoArquivo, f);
    buffer[tamanhoArquivo] = '\0';

    fclose(f);

    parametros->json = cJSON_Parse(buffer);
    free(buffer);
}