#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "structs.h"
#include "cJSON.h"
#include "processData.h"


void *carregarJson(void *args)
{
    ARGSCARREGARJSON *parametros = (ARGSCARREGARJSON *)args;

    FILE *f = fopen(parametros->nomeArquivo, "rb");

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

//Calcular para cada cidade:
//Temperatura - Máx, Min e Media, Data/Hora da máx e da min;
//Umidade - Máx, Min e Media, Data/Hora da máx e da min;
//Pressão atmosférica - Máx, Min e Media, Data/Hora da máx e da min;
//Bateria - Inicial, final, consumo;
//Spreading factors 

void *processarJson(void *args)
{
    ARGSPROCESSARJSON *parametros = (ARGSPROCESSARJSON *)args;

    cJSON *json = parametros->json;
    cJSON* horarioFinal = cJSON_GetObjectItem(cJSON_GetArrayItem(json, 0),"payload_date");

    sscanf(horarioFinal->valuestring, "%[^T]", parametros->periodoFim);
    parametros->periodoFim[SIZE_DATA-1] = '\0';

    cJSON *itemArray;

    cJSON_ArrayForEach(itemArray,json){
        cJSON *dadosBrutos = cJSON_Parse(cJSON_GetObjectItem(itemArray,"brute_data")->valuestring);

        if(!itemArray->next){
            sscanf(cJSON_GetObjectItem(itemArray,"payload_date")->valuestring, "%[^T]", parametros->periodoInicio);
            parametros->periodoInicio[SIZE_DATA-1] = '\0';
        }

        bool caxias = true;

        if(strstr(cJSON_GetObjectItem(dadosBrutos,"device_name")->valuestring, "Bento"))
        {
            caxias = false;
            parametros->estatisticasBento.numeroRegistros++;
        }
        else{
            parametros->estatisticasCaxias.numeroRegistros++;
        }

        cJSON *itemDadosBrutos;
        cJSON_ArrayForEach(itemDadosBrutos, cJSON_GetObjectItem(dadosBrutos,"data")){
            
            cJSON *variavel = cJSON_GetObjectItem(itemDadosBrutos,"variable");

            if(!strcmp(variavel->valuestring, "temperature"))
            {
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos,"time");
                if(caxias)
                {
                    parametros->estatisticasCaxias.dadosTemperatura.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasCaxias.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasCaxias.dadosTemperatura.minima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
                else{
                    parametros->estatisticasBento.dadosTemperatura.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasBento.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasBento.dadosTemperatura.minima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
            }
            else if(!strcmp(variavel->valuestring, "humidity"))
            {   
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos,"time");
                if(caxias)
                {
                    parametros->estatisticasCaxias.dadosUmidade.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasCaxias.dadosUmidade.maxima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasCaxias.dadosUmidade.minima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
                else{
                    parametros->estatisticasBento.dadosUmidade.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasBento.dadosUmidade.maxima)
                    {
                        parametros->estatisticasBento.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasBento.dadosUmidade.minima)
                    {
                        parametros->estatisticasBento.dadosUmidade.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
            }
            else if(!strcmp(variavel->valuestring, "airpressure"))
            {   
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos,"time");
                if(caxias)
                {
                    parametros->estatisticasCaxias.dadosPressaoAtmosferica.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
                else{
                    parametros->estatisticasBento.dadosPressaoAtmosferica.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasBento.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasBento.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
            }
            else if(!strcmp(variavel->valuestring, "batterylevel"))
            {   
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                if(caxias)
                {
                    if(valor->valuedouble > parametros->estatisticasCaxias.dadosBateria.inicial)
                    {
                        parametros->estatisticasCaxias.dadosBateria.inicial = valor->valuedouble;
                    }
                }
                else{
                    if(valor->valuedouble > parametros->estatisticasBento.dadosBateria.inicial)
                    {
                        parametros->estatisticasBento.dadosBateria.inicial = valor->valuedouble;
                    }
                }
            }
        }
    }
}

void *processarJsonMqtt(void *args)
{
    ARGSPROCESSARJSONMQTT *parametros = (ARGSPROCESSARJSONMQTT *)args;

    cJSON *json = parametros->json;
    cJSON* horarioFinal = cJSON_GetObjectItem(cJSON_GetArrayItem(json, 0),"created_at");

    sscanf(horarioFinal->valuestring, "%[^T]", parametros->periodoFim);
    parametros->periodoFim[SIZE_DATA-1] = '\0';

    cJSON *itemArray;
    cJSON_ArrayForEach(itemArray, json)
    {
        cJSON *dadosBrutos = cJSON_Parse(cJSON_GetObjectItem(itemArray,"payload")->valuestring);

        if(!itemArray->next){
            sscanf(cJSON_GetObjectItem(itemArray,"created_at")->valuestring, "%[^T]", parametros->periodoInicio);
            parametros->periodoInicio[SIZE_DATA-1] = '\0';
        }

        bool caxias = true;

        if(strstr(cJSON_GetObjectItem(dadosBrutos,"device_name")->valuestring, "Bento"))
        {
            caxias = false;
            parametros->estatisticasBento.numeroRegistros++;
        }
        else{
            parametros->estatisticasCaxias.numeroRegistros++;
        }

        cJSON *itemDadosBrutos;
        cJSON_ArrayForEach(itemDadosBrutos, cJSON_GetObjectItem(dadosBrutos,"data"))
        {
             cJSON *variavel = cJSON_GetObjectItem(itemDadosBrutos,"variable");

            if(!strcmp(variavel->valuestring, "temperature"))
            {
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos,"time");
                if(caxias)
                {
                    parametros->estatisticasCaxias.dadosTemperatura.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasCaxias.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasCaxias.dadosTemperatura.minima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
                else{
                    parametros->estatisticasBento.dadosTemperatura.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasBento.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasBento.dadosTemperatura.minima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
            }
            else if(!strcmp(variavel->valuestring, "humidity"))
            {   
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos,"time");
                if(caxias)
                {
                    parametros->estatisticasCaxias.dadosUmidade.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasCaxias.dadosUmidade.maxima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasCaxias.dadosUmidade.minima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
                else{
                    parametros->estatisticasBento.dadosUmidade.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasBento.dadosUmidade.maxima)
                    {
                        parametros->estatisticasBento.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasBento.dadosUmidade.minima)
                    {
                        parametros->estatisticasBento.dadosUmidade.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
            }
            else if(!strcmp(variavel->valuestring, "airpressure"))
            {   
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos,"time");
                if(caxias)
                {
                    parametros->estatisticasCaxias.dadosPressaoAtmosferica.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
                else{
                    parametros->estatisticasBento.dadosPressaoAtmosferica.media += valor->valuedouble;
                    if(valor->valuedouble > parametros->estatisticasBento.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA-1] = '\0';
                    }
                    if(valor->valuedouble < parametros->estatisticasBento.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.minima= valor->valuedouble;
                        sscanf(dataHora->valuestring,"%[^.]",parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA-1] = '\0';
                    }
                }
            }
            else if(!strcmp(variavel->valuestring, "batterylevel"))
            {   
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                if(caxias)
                {
                    if(valor->valuedouble < parametros->estatisticasCaxias.dadosBateria.final)
                    {
                        parametros->estatisticasCaxias.dadosBateria.final = valor->valuedouble;
                    }
                }
                else{
                    if(valor->valuedouble < parametros->estatisticasBento.dadosBateria.final)
                    {
                        parametros->estatisticasBento.dadosBateria.final = valor->valuedouble;
                    }
                }
            }
            else if(!strcmp(variavel->valuestring, "lora_spreading_factor"))
            {   
                int i;
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos,"value");
                if(caxias)
                {
                    bool estaPresente = false;
                    for (i = 0; i < parametros->estatisticasCaxias.dadosSpreadingFactors.nItens; i++)
                    {
                        if(valor->valueint == parametros->estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[i])
                        {
                            estaPresente = true;
                            break;
                        }
                    }
                    if(!estaPresente)
                    {
                        parametros->estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[parametros->estatisticasCaxias.dadosSpreadingFactors.nItens++] = valor->valueint;
                    }
                }
                else{
                    bool estaPresente = false;
                    for (i = 0; i < parametros->estatisticasBento.dadosSpreadingFactors.nItens; i++)
                    {
                        if(valor->valueint == parametros->estatisticasBento.dadosSpreadingFactors.spreadingFactors[i])
                        {
                            estaPresente = true;
                            break;
                        }
                    }
                    if(!estaPresente)
                    {
                        parametros->estatisticasBento.dadosSpreadingFactors.spreadingFactors[parametros->estatisticasBento.dadosSpreadingFactors.nItens++] = valor->valueint;
                    }
                }
            }
        }
    }
}