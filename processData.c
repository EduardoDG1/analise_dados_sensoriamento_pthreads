#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "structs.h"
#include "cJSON.h"
#include "processData.h"
#include <pthread.h>   
#include <semaphore.h> 
#define JANELADUPLICACAO 10

void *carregarJson(void *args)
{
    ARGSCARREGARJSON *parametros = (ARGSCARREGARJSON *)args;
    SharedBuffer *sb = parametros->shared;

    char msg[LOG_MSG_SIZE];
    snprintf(msg, LOG_MSG_SIZE, "[LEITORA] Thread iniciada: %s", parametros->nomeArquivo);
    log_push(parametros->lq, msg);

    FILE *f = fopen(parametros->nomeArquivo, "rb");
    if (!f)
    {
        printf("Erro ao abrir arquivo!\n");
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    long tamanhoArquivo = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = (char *)malloc(tamanhoArquivo + 1);
    fread(buffer, 1, tamanhoArquivo, f);
    buffer[tamanhoArquivo] = '\0';
    fclose(f);

    cJSON *array = cJSON_Parse(buffer);
    free(buffer);

    pthread_mutex_lock(&sb->mutex);
    sb->total_registros = cJSON_GetArraySize(array);
    pthread_mutex_unlock(&sb->mutex);

    // janela deslizante de deduplicação
    ItemDeduplicacao janela[JANELADUPLICACAO];
    memset(janela, 0, sizeof(janela));
    int nTimestamps = 0;

    cJSON *item = NULL;
    
    int posicao = 0;

    cJSON_ArrayForEach(item, array)
    {
        posicao++;
        ItemBuffer entry;
        entry.ultimo = 0;

        cJSON *payloadNode = cJSON_GetObjectItem(item, "brute_data");
        if (!payloadNode) payloadNode = cJSON_GetObjectItem(item, "payload");
        if (!payloadNode) { printf("campo payload nao encontrado\n"); exit(1); }

        entry.payloadStr = strdup(payloadNode->valuestring);

        cJSON *dateNode = cJSON_GetObjectItem(item, "payload_date");
        if (!dateNode) dateNode = cJSON_GetObjectItem(item, "created_at");
        sscanf(dateNode->valuestring, "%[^T]", entry.payloadDate);
        entry.payloadDate[SIZE_DATA - 1] = '\0';

        // extrai o payload_id do item raiz
        cJSON *payloadIdNode = cJSON_GetObjectItem(item, "payload_id");
        int payloadId = payloadIdNode ? payloadIdNode->valueint : -1;

        // extrai timestamp do primeiro dado pra deduplicação
        char timestamp[30] = {0};
        cJSON *payloadTemp = cJSON_Parse(payloadNode->valuestring);
        if (payloadTemp)
        {
            cJSON *primeiroDado = cJSON_GetArrayItem(cJSON_GetObjectItem(payloadTemp, "data"), 0);
            if (primeiroDado)
            {
                cJSON *timeNode = cJSON_GetObjectItem(primeiroDado, "time");
                if (timeNode)
                    strncpy(timestamp, timeNode->valuestring, 29);
            }
            cJSON_Delete(payloadTemp);
        }

        bool duplicata = false;
        int i;
        for (i = 0; i < nTimestamps && i < JANELADUPLICACAO; i++)
        {           
            if (!strcmp(timestamp, janela[i].timestamp))
            {
                
                duplicata = true;
                snprintf(msg, LOG_MSG_SIZE, "[LEITORA] Registro duplicado ignorado - payload_id: %d é duplicado de payload_id: %d",
                    payloadId, janela[i].payloadId);
                log_push(parametros->lq, msg);
                break;
            }
        }

        if (duplicata)
        {
            pthread_mutex_lock(&sb->mutex);
            sb->registros_duplicados++;
            pthread_mutex_unlock(&sb->mutex);
            free(entry.payloadStr);
            continue;
        }

        // registra na janela deslizante
        strncpy(janela[nTimestamps % JANELADUPLICACAO].timestamp, timestamp, 29);
        janela[nTimestamps % JANELADUPLICACAO].payloadId = payloadId;
        nTimestamps++;

        sem_wait(&sb->sem_empty);
        pthread_mutex_lock(&sb->mutex);
        sb->buffer[sb->head] = entry;
        sb->head = (sb->head + 1) % BUFFER_SIZE;
        sb->count++;
        sb->registros_lidos++;
        if (sb->registros_lidos % 100 == 0)
        {
            snprintf(msg, LOG_MSG_SIZE, "[LEITORA] %s - %d/%d registros lidos",
                parametros->nomeArquivo, sb->registros_lidos, sb->total_registros);
            log_push(parametros->lq, msg);
        }
        pthread_mutex_unlock(&sb->mutex);
        sem_post(&sb->sem_full);
    }

    // sentinela
    ItemBuffer fim = {0};
    fim.ultimo = 1;
    sem_wait(&sb->sem_empty);
    pthread_mutex_lock(&sb->mutex);
    sb->buffer[sb->head] = fim;
    sb->head = (sb->head + 1) % BUFFER_SIZE;
    sb->count++;
    sb->leitura_concluida = 1;
    pthread_mutex_unlock(&sb->mutex);
    sem_post(&sb->sem_full);

    cJSON_Delete(array);

    snprintf(msg, LOG_MSG_SIZE, "[LEITORA] Thread finalizada: %s - %d registros lidos",
        parametros->nomeArquivo, sb->registros_lidos + sb->registros_duplicados);
    log_push(parametros->lq, msg);

    return NULL;
}

void *processarJson(void *args)
{
    ARGSPROCESSARJSON *parametros = (ARGSPROCESSARJSON *)args;
    SharedBuffer *sb = parametros->shared;
    int primeiroItem = 1;

    char msg[LOG_MSG_SIZE];

    snprintf(msg, LOG_MSG_SIZE, "[CALCULADORA 1] Thread iniciada");
    log_push(parametros->lq, msg);


    while (1)
    {
        // consome um item do buffer
        sem_wait(&sb->sem_full);
        pthread_mutex_lock(&sb->mutex);
        ItemBuffer entry = sb->buffer[sb->tail];
        sb->tail = (sb->tail + 1) % BUFFER_SIZE;
        sb->count--;
        pthread_mutex_unlock(&sb->mutex);
        sem_post(&sb->sem_empty);

        // sentinela: thread leitora terminou
        if (entry.ultimo) break;

        if (primeiroItem) {
            sscanf(entry.payloadDate, "%[^T]", parametros->periodoFim);
            parametros->periodoFim[SIZE_DATA - 1] = '\0';
            primeiroItem = 0;
        }
        sscanf(entry.payloadDate, "%[^T]", parametros->periodoInicio);
        parametros->periodoInicio[SIZE_DATA - 1] = '\0';

        // parse do payload bruto
        cJSON *dadosBrutos = cJSON_Parse(entry.payloadStr);
        free(entry.payloadStr);

        bool caxias = true;
        if (strstr(cJSON_GetObjectItem(dadosBrutos, "device_name")->valuestring, "Bento"))
        {
            caxias = false;
            parametros->estatisticasBento.numeroRegistros++;
        }
        else
        {
            parametros->estatisticasCaxias.numeroRegistros++;
        }

        cJSON *itemDadosBrutos;
        cJSON_ArrayForEach(itemDadosBrutos, cJSON_GetObjectItem(dadosBrutos, "data"))
        {
            cJSON *variavel = cJSON_GetObjectItem(itemDadosBrutos, "variable");
            if (!variavel) { printf("erro ao parsear variable"); continue;} //teste
            if (!strcmp(variavel->valuestring, "temperature"))
            {
                cJSON *valor   = cJSON_GetObjectItem(itemDadosBrutos, "value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos, "time");
                if (caxias)
                {
                    parametros->estatisticasCaxias.dadosTemperatura.media += valor->valuedouble;
                    parametros->estatisticasCaxias.contadores.temperatura++;
                    if (valor->valuedouble > parametros->estatisticasCaxias.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasCaxias.dadosTemperatura.minima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
                else
                {
                    parametros->estatisticasBento.dadosTemperatura.media += valor->valuedouble;
                    parametros->estatisticasBento.contadores.temperatura++;
                    if (valor->valuedouble > parametros->estatisticasBento.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasBento.dadosTemperatura.minima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
            }
            else if (!strcmp(variavel->valuestring, "humidity"))
            {
                cJSON *valor    = cJSON_GetObjectItem(itemDadosBrutos, "value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos, "time");
                if (caxias)
                {
                    parametros->estatisticasCaxias.dadosUmidade.media += valor->valuedouble;
                    parametros->estatisticasCaxias.contadores.umidade++;
                    if (valor->valuedouble > parametros->estatisticasCaxias.dadosUmidade.maxima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasCaxias.dadosUmidade.minima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
                else
                {
                    parametros->estatisticasBento.dadosUmidade.media += valor->valuedouble;
                    parametros->estatisticasBento.contadores.umidade++;
                    if (valor->valuedouble > parametros->estatisticasBento.dadosUmidade.maxima)
                    {
                        parametros->estatisticasBento.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasBento.dadosUmidade.minima)
                    {
                        parametros->estatisticasBento.dadosUmidade.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
            }
            else if (!strcmp(variavel->valuestring, "airpressure"))
            {
                cJSON *valor    = cJSON_GetObjectItem(itemDadosBrutos, "value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos, "time");
                if (caxias)
                {
                    parametros->estatisticasCaxias.dadosPressaoAtmosferica.media += valor->valuedouble;
                    parametros->estatisticasCaxias.contadores.pressaoAtmosferica++;
                    if (valor->valuedouble > parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
                else
                {
                    parametros->estatisticasBento.dadosPressaoAtmosferica.media += valor->valuedouble;
                    parametros->estatisticasBento.contadores.pressaoAtmosferica++;
                    if (valor->valuedouble > parametros->estatisticasBento.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasBento.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
            }
            else if (!strcmp(variavel->valuestring, "batterylevel"))
            {
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos, "value");
                if (caxias)
                {
                    if (valor->valuedouble > parametros->estatisticasCaxias.dadosBateria.inicial)
                        parametros->estatisticasCaxias.dadosBateria.inicial = valor->valuedouble;
                }
                else
                {
                    if (valor->valuedouble > parametros->estatisticasBento.dadosBateria.inicial)
                        parametros->estatisticasBento.dadosBateria.inicial = valor->valuedouble;
                }
            }
        }

        cJSON_Delete(dadosBrutos);

        pthread_mutex_lock(&sb->mutex);
        sb->registros_processados++;

        // progresso a cada 100 registros
        if (sb->registros_processados % 100 == 0) {
            snprintf(msg, LOG_MSG_SIZE, "[CALCULADORA 1] %d/%d registros processados",
                sb->registros_processados, sb->total_registros);
            log_push(parametros->lq, msg);
        }
        pthread_mutex_unlock(&sb->mutex);
    }

    snprintf(msg, LOG_MSG_SIZE, "[CALCULADORA 1] Thread finalizada - %d registros processados",
    sb->registros_processados);
    log_push(parametros->lq, msg);

    return NULL;
}

void *processarJsonMqtt(void *args)
{
    
    ARGSPROCESSARJSONMQTT *parametros = (ARGSPROCESSARJSONMQTT *)args;
    SharedBuffer *sb = parametros->shared;
    int primeiroItem = 1;

    char msg[LOG_MSG_SIZE];
    snprintf(msg, LOG_MSG_SIZE, "[CALCULADORA 2] Thread iniciada");
    log_push(parametros->lq, msg);

    while (1)
    {
        // consome um item do buffer
        sem_wait(&sb->sem_full);
        pthread_mutex_lock(&sb->mutex);
        ItemBuffer entry = sb->buffer[sb->tail];
        sb->tail = (sb->tail + 1) % BUFFER_SIZE;
        sb->count--;
        pthread_mutex_unlock(&sb->mutex);
        sem_post(&sb->sem_empty);

        // sentinela: leitora terminou
        if (entry.ultimo) break;

        // captura período
        if (primeiroItem) {
            sscanf(entry.payloadDate, "%[^T]", parametros->periodoInicio);
            parametros->periodoInicio[SIZE_DATA - 1] = '\0';
            primeiroItem = 0;
        }
        sscanf(entry.payloadDate, "%[^T]", parametros->periodoFim);
        parametros->periodoFim[SIZE_DATA - 1] = '\0';

        // parse do payload bruto
        cJSON *dadosBrutos = cJSON_Parse(entry.payloadStr);
        free(entry.payloadStr);

        if(!dadosBrutos) {printf("Erro ao parsear payload: %s\n", entry.payloadDate); continue;}

        bool caxias = true;
        if (strstr(cJSON_GetObjectItem(dadosBrutos, "device_name")->valuestring, "Bento"))
        {
            caxias = false;
            parametros->estatisticasBento.numeroRegistros++;
        }
        else
        {
            parametros->estatisticasCaxias.numeroRegistros++;
        }

        cJSON *itemDadosBrutos;
        cJSON_ArrayForEach(itemDadosBrutos, cJSON_GetObjectItem(dadosBrutos, "data"))
        {
            cJSON *variavel = cJSON_GetObjectItem(itemDadosBrutos, "variable");
            if (!variavel) { printf("erro ao parsear variable"); continue;} //teste
            if (!strcmp(variavel->valuestring, "temperature"))
            {
                cJSON *valor    = cJSON_GetObjectItem(itemDadosBrutos, "value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos, "time");
                if (caxias)
                {
                    parametros->estatisticasCaxias.dadosTemperatura.media += valor->valuedouble;
                    parametros->estatisticasCaxias.contadores.temperatura++;
                    if (valor->valuedouble > parametros->estatisticasCaxias.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasCaxias.dadosTemperatura.minima)
                    {
                        parametros->estatisticasCaxias.dadosTemperatura.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
                else
                {
                    parametros->estatisticasBento.dadosTemperatura.media += valor->valuedouble;
                    parametros->estatisticasBento.contadores.temperatura++;
                    if (valor->valuedouble > parametros->estatisticasBento.dadosTemperatura.maxima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasBento.dadosTemperatura.minima)
                    {
                        parametros->estatisticasBento.dadosTemperatura.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosTemperatura.dataHoraMinima);
                        parametros->estatisticasBento.dadosTemperatura.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
            }
            else if (!strcmp(variavel->valuestring, "humidity"))
            {
                cJSON *valor    = cJSON_GetObjectItem(itemDadosBrutos, "value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos, "time");
                if (caxias)
                {
                    parametros->estatisticasCaxias.dadosUmidade.media += valor->valuedouble;
                    parametros->estatisticasCaxias.contadores.umidade++;
                    if (valor->valuedouble > parametros->estatisticasCaxias.dadosUmidade.maxima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasCaxias.dadosUmidade.minima)
                    {
                        parametros->estatisticasCaxias.dadosUmidade.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosUmidade.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
                else
                {
                    parametros->estatisticasBento.dadosUmidade.media += valor->valuedouble;
                    parametros->estatisticasBento.contadores.umidade++;
                    if (valor->valuedouble > parametros->estatisticasBento.dadosUmidade.maxima)
                    {
                        parametros->estatisticasBento.dadosUmidade.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosUmidade.dataHoraMaxima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasBento.dadosUmidade.minima)
                    {
                        parametros->estatisticasBento.dadosUmidade.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosUmidade.dataHoraMinima);
                        parametros->estatisticasBento.dadosUmidade.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
            }
            else if (!strcmp(variavel->valuestring, "airpressure"))
            {
                cJSON *valor    = cJSON_GetObjectItem(itemDadosBrutos, "value");
                cJSON *dataHora = cJSON_GetObjectItem(itemDadosBrutos, "time");
                if (caxias)
                {
                    parametros->estatisticasCaxias.dadosPressaoAtmosferica.media += valor->valuedouble;
                    parametros->estatisticasCaxias.contadores.pressaoAtmosferica++;
                    if (valor->valuedouble > parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
                else
                {
                    parametros->estatisticasBento.dadosPressaoAtmosferica.media += valor->valuedouble;
                    parametros->estatisticasBento.contadores.pressaoAtmosferica++;
                    if (valor->valuedouble > parametros->estatisticasBento.dadosPressaoAtmosferica.maxima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.maxima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima[SIZE_DATAHORA - 1] = '\0';
                    }
                    if (valor->valuedouble < parametros->estatisticasBento.dadosPressaoAtmosferica.minima)
                    {
                        parametros->estatisticasBento.dadosPressaoAtmosferica.minima = valor->valuedouble;
                        sscanf(dataHora->valuestring, "%[^.]", parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima);
                        parametros->estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima[SIZE_DATAHORA - 1] = '\0';
                    }
                }
            }
            else if (!strcmp(variavel->valuestring, "batterylevel"))
            {
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos, "value");
                if (caxias)
                {
                    // mqtt: busca o mínimo (bateria final)
                    if (valor->valuedouble < parametros->estatisticasCaxias.dadosBateria.final)
                        parametros->estatisticasCaxias.dadosBateria.final = valor->valuedouble;
                }
                else
                {
                    if (valor->valuedouble < parametros->estatisticasBento.dadosBateria.final)
                        parametros->estatisticasBento.dadosBateria.final = valor->valuedouble;
                }
            }
            else if (!strcmp(variavel->valuestring, "lora_spreading_factor"))
            {
                int i;
                cJSON *valor = cJSON_GetObjectItem(itemDadosBrutos, "value");
                if (caxias)
                {
                    bool estaPresente = false;
                    for (i = 0; i < parametros->estatisticasCaxias.dadosSpreadingFactors.nItens; i++)
                    {
                        if (valor->valueint == parametros->estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[i])
                        {
                            estaPresente = true;
                            break;
                        }
                    }
                    if (!estaPresente)
                        parametros->estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[parametros->estatisticasCaxias.dadosSpreadingFactors.nItens++] = valor->valueint;
                }
                else
                {
                    bool estaPresente = false;
                    for (i = 0; i < parametros->estatisticasBento.dadosSpreadingFactors.nItens; i++)
                    {
                        if (valor->valueint == parametros->estatisticasBento.dadosSpreadingFactors.spreadingFactors[i])
                        {
                            estaPresente = true;
                            break;
                        }
                    }
                    if (!estaPresente)
                        parametros->estatisticasBento.dadosSpreadingFactors.spreadingFactors[parametros->estatisticasBento.dadosSpreadingFactors.nItens++] = valor->valueint;
                }
            }
        }

        cJSON_Delete(dadosBrutos);

        pthread_mutex_lock(&sb->mutex);
        sb->registros_processados++;
        if (sb->registros_processados % 100 == 0) {
            snprintf(msg, LOG_MSG_SIZE, "[CALCULADORA 2] %d/%d registros processados",
                sb->registros_processados, sb->total_registros);
            log_push(parametros->lq, msg);
        }
        pthread_mutex_unlock(&sb->mutex);
    }

    snprintf(msg, LOG_MSG_SIZE, "[CALCULADORA 2] Thread finalizada - %d registros processados",
        sb->registros_processados);
    log_push(parametros->lq, msg);


    return NULL;
}


ESTATISTICASCAXIAS calcularEstatisticasCaxias(ESTATISTICASCAXIAS estatisticasCaxias1, ESTATISTICASCAXIAS estatisticasCaxias2)
{
    ESTATISTICASCAXIAS estatisticasCaxiasFinal;

    estatisticasCaxiasFinal.numeroRegistros = estatisticasCaxias1.numeroRegistros + estatisticasCaxias2.numeroRegistros;
    
    estatisticasCaxiasFinal.contadores.temperatura = 
        estatisticasCaxias1.contadores.temperatura + estatisticasCaxias2.contadores.temperatura;
    estatisticasCaxiasFinal.contadores.umidade = 
        estatisticasCaxias1.contadores.umidade + estatisticasCaxias2.contadores.umidade;
    estatisticasCaxiasFinal.contadores.pressaoAtmosferica = 
        estatisticasCaxias1.contadores.pressaoAtmosferica + estatisticasCaxias2.contadores.pressaoAtmosferica;
    
    //Temperatura
    estatisticasCaxiasFinal.dadosTemperatura.media = (estatisticasCaxias1.dadosTemperatura.media + estatisticasCaxias2.dadosTemperatura.media) / estatisticasCaxiasFinal.contadores.temperatura;
    if(estatisticasCaxias1.dadosTemperatura.maxima > estatisticasCaxias2.dadosTemperatura.maxima)
    {
        estatisticasCaxiasFinal.dadosTemperatura.maxima = estatisticasCaxias1.dadosTemperatura.maxima;
        strcpy(estatisticasCaxiasFinal.dadosTemperatura.dataHoraMaxima, estatisticasCaxias1.dadosTemperatura.dataHoraMaxima);
    } 
    else
    {
        estatisticasCaxiasFinal.dadosTemperatura.maxima = estatisticasCaxias2.dadosTemperatura.maxima;
        strcpy(estatisticasCaxiasFinal.dadosTemperatura.dataHoraMaxima, estatisticasCaxias2.dadosTemperatura.dataHoraMaxima);
    }

    if(estatisticasCaxias1.dadosTemperatura.minima < estatisticasCaxias2.dadosTemperatura.minima)
    {
        estatisticasCaxiasFinal.dadosTemperatura.minima = estatisticasCaxias1.dadosTemperatura.minima;
        strcpy(estatisticasCaxiasFinal.dadosTemperatura.dataHoraMinima, estatisticasCaxias1.dadosTemperatura.dataHoraMinima);
    } 
    else
    {
        estatisticasCaxiasFinal.dadosTemperatura.minima = estatisticasCaxias2.dadosTemperatura.minima;
        strcpy(estatisticasCaxiasFinal.dadosTemperatura.dataHoraMinima, estatisticasCaxias2.dadosTemperatura.dataHoraMinima);
    }    

    //Umidade
    estatisticasCaxiasFinal.dadosUmidade.media = (estatisticasCaxias1.dadosUmidade.media + estatisticasCaxias2.dadosUmidade.media) / estatisticasCaxiasFinal.contadores.umidade;
    //estatisticasCaxiasFinal.dadosUmidade.media = (estatisticasCaxias1.dadosUmidade.media + estatisticasCaxias2.dadosUmidade.media)/estatisticasCaxiasFinal.contadores.umidade;
    if(estatisticasCaxias1.dadosUmidade.maxima > estatisticasCaxias2.dadosUmidade.maxima)
    {
        estatisticasCaxiasFinal.dadosUmidade.maxima = estatisticasCaxias1.dadosUmidade.maxima;
        strcpy(estatisticasCaxiasFinal.dadosUmidade.dataHoraMaxima, estatisticasCaxias1.dadosUmidade.dataHoraMaxima);
    } 
    else
    {
        estatisticasCaxiasFinal.dadosUmidade.maxima = estatisticasCaxias2.dadosUmidade.maxima;
        strcpy(estatisticasCaxiasFinal.dadosUmidade.dataHoraMaxima, estatisticasCaxias2.dadosUmidade.dataHoraMaxima);
    }

    if(estatisticasCaxias1.dadosUmidade.minima < estatisticasCaxias2.dadosUmidade.minima)
    {
        estatisticasCaxiasFinal.dadosUmidade.minima = estatisticasCaxias1.dadosUmidade.minima;
        strcpy(estatisticasCaxiasFinal.dadosUmidade.dataHoraMinima, estatisticasCaxias1.dadosUmidade.dataHoraMinima);
    } 
    else
    {
        estatisticasCaxiasFinal.dadosUmidade.minima = estatisticasCaxias2.dadosUmidade.minima;
        strcpy(estatisticasCaxiasFinal.dadosUmidade.dataHoraMinima, estatisticasCaxias2.dadosUmidade.dataHoraMinima);
    }   

    //PressaoAtmosferica
    estatisticasCaxiasFinal.dadosPressaoAtmosferica.media = (estatisticasCaxias1.dadosPressaoAtmosferica.media + estatisticasCaxias2.dadosPressaoAtmosferica.media) / estatisticasCaxiasFinal.contadores.pressaoAtmosferica;
    if(estatisticasCaxias1.dadosPressaoAtmosferica.maxima > estatisticasCaxias2.dadosPressaoAtmosferica.maxima)
    {
        estatisticasCaxiasFinal.dadosPressaoAtmosferica.maxima = estatisticasCaxias1.dadosPressaoAtmosferica.maxima;
        strcpy(estatisticasCaxiasFinal.dadosPressaoAtmosferica.dataHoraMaxima, estatisticasCaxias1.dadosPressaoAtmosferica.dataHoraMaxima);
    } 
    else
    {
        estatisticasCaxiasFinal.dadosPressaoAtmosferica.maxima = estatisticasCaxias2.dadosPressaoAtmosferica.maxima;
        strcpy(estatisticasCaxiasFinal.dadosPressaoAtmosferica.dataHoraMaxima, estatisticasCaxias2.dadosPressaoAtmosferica.dataHoraMaxima);
    }

    if(estatisticasCaxias1.dadosPressaoAtmosferica.minima < estatisticasCaxias2.dadosPressaoAtmosferica.minima)
    {
        estatisticasCaxiasFinal.dadosPressaoAtmosferica.minima = estatisticasCaxias1.dadosPressaoAtmosferica.minima;
        strcpy(estatisticasCaxiasFinal.dadosPressaoAtmosferica.dataHoraMinima, estatisticasCaxias1.dadosPressaoAtmosferica.dataHoraMinima);
    } 
    else
    {
        estatisticasCaxiasFinal.dadosPressaoAtmosferica.minima = estatisticasCaxias2.dadosPressaoAtmosferica.minima;
        strcpy(estatisticasCaxiasFinal.dadosPressaoAtmosferica.dataHoraMinima, estatisticasCaxias2.dadosPressaoAtmosferica.dataHoraMinima);
    }   

    //Bateria
    estatisticasCaxiasFinal.dadosBateria.inicial = estatisticasCaxias1.dadosBateria.inicial;
    estatisticasCaxiasFinal.dadosBateria.final = estatisticasCaxias2.dadosBateria.final;
    estatisticasCaxiasFinal.dadosBateria.consumo = estatisticasCaxiasFinal.dadosBateria.inicial - estatisticasCaxiasFinal.dadosBateria.final;

    //SpreadingFactors
    int i;
    estatisticasCaxiasFinal.dadosSpreadingFactors.nItens = estatisticasCaxias2.dadosSpreadingFactors.nItens;
    for (i = 0; i < estatisticasCaxiasFinal.dadosSpreadingFactors.nItens; i++)
    {
        estatisticasCaxiasFinal.dadosSpreadingFactors.spreadingFactors[i] = estatisticasCaxias2.dadosSpreadingFactors.spreadingFactors[i];
    }
    
    return estatisticasCaxiasFinal;
}

ESTATISTICASBENTO calcularEstatisticasBento(ESTATISTICASBENTO estatisticasBento1, ESTATISTICASBENTO estatisticasBento2)
{
        ESTATISTICASBENTO estatisticasBentoFinal;

    estatisticasBentoFinal.numeroRegistros = estatisticasBento1.numeroRegistros + estatisticasBento2.numeroRegistros;

    estatisticasBentoFinal.contadores.temperatura = 
        estatisticasBento1.contadores.temperatura + estatisticasBento2.contadores.temperatura;
    estatisticasBentoFinal.contadores.umidade = 
        estatisticasBento1.contadores.umidade + estatisticasBento2.contadores.umidade;
    estatisticasBentoFinal.contadores.pressaoAtmosferica = 
        estatisticasBento1.contadores.pressaoAtmosferica + estatisticasBento2.contadores.pressaoAtmosferica;
    
    //Temperatura
    estatisticasBentoFinal.dadosTemperatura.media = (estatisticasBento1.dadosTemperatura.media + estatisticasBento2.dadosTemperatura.media)/estatisticasBentoFinal.contadores.temperatura;
    if(estatisticasBento1.dadosTemperatura.maxima > estatisticasBento2.dadosTemperatura.maxima)
    {
        estatisticasBentoFinal.dadosTemperatura.maxima = estatisticasBento1.dadosTemperatura.maxima;
        strcpy(estatisticasBentoFinal.dadosTemperatura.dataHoraMaxima, estatisticasBento1.dadosTemperatura.dataHoraMaxima);
    } 
    else
    {
        estatisticasBentoFinal.dadosTemperatura.maxima = estatisticasBento2.dadosTemperatura.maxima;
        strcpy(estatisticasBentoFinal.dadosTemperatura.dataHoraMaxima, estatisticasBento2.dadosTemperatura.dataHoraMaxima);
    }

    if(estatisticasBento1.dadosTemperatura.minima < estatisticasBento2.dadosTemperatura.minima)
    {
        estatisticasBentoFinal.dadosTemperatura.minima = estatisticasBento1.dadosTemperatura.minima;
        strcpy(estatisticasBentoFinal.dadosTemperatura.dataHoraMinima, estatisticasBento1.dadosTemperatura.dataHoraMinima);
    } 
    else
    {
        estatisticasBentoFinal.dadosTemperatura.minima = estatisticasBento2.dadosTemperatura.minima;
        strcpy(estatisticasBentoFinal.dadosTemperatura.dataHoraMinima, estatisticasBento2.dadosTemperatura.dataHoraMinima);
    }    

    //Umidade
    estatisticasBentoFinal.dadosUmidade.media = (estatisticasBento1.dadosUmidade.media + estatisticasBento2.dadosUmidade.media) / estatisticasBentoFinal.contadores.umidade;
    if(estatisticasBento1.dadosUmidade.maxima > estatisticasBento2.dadosUmidade.maxima)
    {
        estatisticasBentoFinal.dadosUmidade.maxima = estatisticasBento1.dadosUmidade.maxima;
        strcpy(estatisticasBentoFinal.dadosUmidade.dataHoraMaxima, estatisticasBento1.dadosUmidade.dataHoraMaxima);
    } 
    else
    {
        estatisticasBentoFinal.dadosUmidade.maxima = estatisticasBento2.dadosUmidade.maxima;
        strcpy(estatisticasBentoFinal.dadosUmidade.dataHoraMaxima, estatisticasBento2.dadosUmidade.dataHoraMaxima);
    }

    if(estatisticasBento1.dadosUmidade.minima < estatisticasBento2.dadosUmidade.minima)
    {
        estatisticasBentoFinal.dadosUmidade.minima = estatisticasBento1.dadosUmidade.minima;
        strcpy(estatisticasBentoFinal.dadosUmidade.dataHoraMinima, estatisticasBento1.dadosUmidade.dataHoraMinima);
    } 
    else
    {
        estatisticasBentoFinal.dadosUmidade.minima = estatisticasBento2.dadosUmidade.minima;
        strcpy(estatisticasBentoFinal.dadosUmidade.dataHoraMinima, estatisticasBento2.dadosUmidade.dataHoraMinima);
    }   

    //PressaoAtmosferica
    estatisticasBentoFinal.dadosPressaoAtmosferica.media = (estatisticasBento1.dadosPressaoAtmosferica.media + estatisticasBento2.dadosPressaoAtmosferica.media) / estatisticasBentoFinal.contadores.pressaoAtmosferica;
    if(estatisticasBento1.dadosPressaoAtmosferica.maxima > estatisticasBento2.dadosPressaoAtmosferica.maxima)
    {
        estatisticasBentoFinal.dadosPressaoAtmosferica.maxima = estatisticasBento1.dadosPressaoAtmosferica.maxima;
        strcpy(estatisticasBentoFinal.dadosPressaoAtmosferica.dataHoraMaxima, estatisticasBento1.dadosPressaoAtmosferica.dataHoraMaxima);
    } 
    else
    {
        estatisticasBentoFinal.dadosPressaoAtmosferica.maxima = estatisticasBento2.dadosPressaoAtmosferica.maxima;
        strcpy(estatisticasBentoFinal.dadosPressaoAtmosferica.dataHoraMaxima, estatisticasBento2.dadosPressaoAtmosferica.dataHoraMaxima);
    }

    if(estatisticasBento1.dadosPressaoAtmosferica.minima < estatisticasBento2.dadosPressaoAtmosferica.minima)
    {
        estatisticasBentoFinal.dadosPressaoAtmosferica.minima = estatisticasBento1.dadosPressaoAtmosferica.minima;
        strcpy(estatisticasBentoFinal.dadosPressaoAtmosferica.dataHoraMinima, estatisticasBento1.dadosPressaoAtmosferica.dataHoraMinima);
    } 
    else
    {
        estatisticasBentoFinal.dadosPressaoAtmosferica.minima = estatisticasBento2.dadosPressaoAtmosferica.minima;
        strcpy(estatisticasBentoFinal.dadosPressaoAtmosferica.dataHoraMinima, estatisticasBento2.dadosPressaoAtmosferica.dataHoraMinima);
    }   

    //Bateria
    estatisticasBentoFinal.dadosBateria.inicial = estatisticasBento1.dadosBateria.inicial;
    estatisticasBentoFinal.dadosBateria.final = estatisticasBento2.dadosBateria.final;
    estatisticasBentoFinal.dadosBateria.consumo = estatisticasBentoFinal.dadosBateria.inicial - estatisticasBentoFinal.dadosBateria.final;

    //SpreadingFactors
    int i;
    estatisticasBentoFinal.dadosSpreadingFactors.nItens = estatisticasBento2.dadosSpreadingFactors.nItens;
    for (i = 0; i < estatisticasBentoFinal.dadosSpreadingFactors.nItens; i++)
    {
        estatisticasBentoFinal.dadosSpreadingFactors.spreadingFactors[i] = estatisticasBento2.dadosSpreadingFactors.spreadingFactors[i];
    }
    
    return estatisticasBentoFinal;
}

void imprimirInformacoesNaTela(ARGSIMPRIMIRDADOS argsImprimirDados){
    printf("============================================================\n");
    printf("ANÁLISE DE DADOS DOS SENSORES - CityLivingLab\n");
    printf("Processamento utilizando pthreads\n");
    printf("============================================================\n\n");
    printf("Arquivo analisado: %s\n", argsImprimirDados.arquivo1);
    printf("Total de registros processados: %d\n", argsImprimirDados.nItensArquivo1);
    printf("Registros ignorados por duplicidade: %d\n", argsImprimirDados.ignoradosArquivo1);

    printf("Período analisado: %02d/%02d/%d a %02d/%02d/%d\n\n",argsImprimirDados.periodoInicioArquivo1.dia,argsImprimirDados.periodoInicioArquivo1.mes,argsImprimirDados.periodoInicioArquivo1.ano, argsImprimirDados.periodoFimArquivo1.dia,argsImprimirDados.periodoFimArquivo1.mes,argsImprimirDados.periodoFimArquivo1.ano);
    printf("Arquivo analisado: %s\n",argsImprimirDados.arquivo2);
    printf("Total de registros processados: %d\n", argsImprimirDados.nItensArquivo2);
    printf("Registros ignorados por duplicidade: %d\n", argsImprimirDados.ignoradosArquivo2);
    printf("Período analisado: %02d/%02d/%d a %02d/%02d/%d\n\n", argsImprimirDados.periodoInicioArquivo2.dia,argsImprimirDados.periodoInicioArquivo2.mes,argsImprimirDados.periodoInicioArquivo2.ano, argsImprimirDados.periodoFimArquivo2.dia,argsImprimirDados.periodoFimArquivo2.mes,argsImprimirDados.periodoFimArquivo2.ano);
    printf("------------------------------------------------------------\n");
    printf("TEMPERATURA (°C)\n");
    printf("------------------------------------------------------------\n");
    printf("Cidade            | Mínima  | Data/Hora             | Máxima | Data/Hora             | Média\n");
    printf("-----------------------------------------------------------------------------------------------\n");
    
    DATAHORA dataHoraMinimaCaxias, dataHoraMaximaCaxias, dataHoraMinimaBento, dataHoraMaximaBento;

    sscanf(argsImprimirDados.estatisticasCaxias.dadosTemperatura.dataHoraMinima, "%d-%d-%dT%d:%d:%d", &dataHoraMinimaCaxias.ano,&dataHoraMinimaCaxias.mes,&dataHoraMinimaCaxias.dia,&dataHoraMinimaCaxias.hora,&dataHoraMinimaCaxias.minuto,&dataHoraMinimaCaxias.segundo);
    sscanf(argsImprimirDados.estatisticasCaxias.dadosTemperatura.dataHoraMaxima, "%d-%d-%dT%d:%d:%d", &dataHoraMaximaCaxias.ano,&dataHoraMaximaCaxias.mes,&dataHoraMaximaCaxias.dia,&dataHoraMaximaCaxias.hora,&dataHoraMaximaCaxias.minuto,&dataHoraMaximaCaxias.segundo);

    sscanf(argsImprimirDados.estatisticasBento.dadosTemperatura.dataHoraMinima, "%d-%d-%dT%d:%d:%d", &dataHoraMinimaBento.ano,&dataHoraMinimaBento.mes,&dataHoraMinimaBento.dia,&dataHoraMinimaBento.hora,&dataHoraMinimaBento.minuto,&dataHoraMinimaBento.segundo);
    sscanf(argsImprimirDados.estatisticasBento.dadosTemperatura.dataHoraMaxima, "%d-%d-%dT%d:%d:%d", &dataHoraMaximaBento.ano,&dataHoraMaximaBento.mes,&dataHoraMaximaBento.dia,&dataHoraMaximaBento.hora,&dataHoraMaximaBento.minuto,&dataHoraMaximaBento.segundo);

    printf("Caxias do Sul     | %.2lf   | %02d/%02d/%d %02d:%02d:%02d   | %.2lf  | %02d/%02d/%d %02d:%02d:%02d   | %.2lf\n", argsImprimirDados.estatisticasCaxias.dadosTemperatura.minima, dataHoraMinimaCaxias.dia, dataHoraMinimaCaxias.mes, dataHoraMinimaCaxias.ano, dataHoraMinimaCaxias.hora, dataHoraMinimaCaxias.minuto, dataHoraMinimaCaxias.segundo, argsImprimirDados.estatisticasCaxias.dadosTemperatura.maxima, dataHoraMaximaCaxias.dia, dataHoraMaximaCaxias.mes, dataHoraMaximaCaxias.ano, dataHoraMaximaCaxias.hora, dataHoraMaximaCaxias.minuto, dataHoraMaximaCaxias.segundo, argsImprimirDados.estatisticasCaxias.dadosTemperatura.media);
    printf("Bento Gonçalves   | %.2lf   | %02d/%02d/%d %02d:%02d:%02d   | %.2lf  | %02d/%02d/%d %02d:%02d:%02d   | %.2lf\n\n", argsImprimirDados.estatisticasBento.dadosTemperatura.minima, dataHoraMinimaBento.dia, dataHoraMinimaBento.mes, dataHoraMinimaBento.ano, dataHoraMinimaBento.hora, dataHoraMinimaBento.minuto, dataHoraMinimaBento.segundo, argsImprimirDados.estatisticasBento.dadosTemperatura.maxima, dataHoraMaximaBento.dia, dataHoraMaximaBento.mes, dataHoraMaximaBento.ano, dataHoraMaximaBento.hora, dataHoraMaximaBento.minuto, dataHoraMaximaBento.segundo, argsImprimirDados.estatisticasBento.dadosTemperatura.media);
    printf("------------------------------------------------------------\n");
    printf("UMIDADE (%%)\n");
    printf("------------------------------------------------------------\n");
    printf("Cidade            | Mínima  | Data/Hora             | Máxima | Data/Hora             | Média\n");
    printf("-----------------------------------------------------------------------------------------------\n");

    sscanf(argsImprimirDados.estatisticasCaxias.dadosUmidade.dataHoraMinima, "%d-%d-%dT%d:%d:%d", &dataHoraMinimaCaxias.ano,&dataHoraMinimaCaxias.mes,&dataHoraMinimaCaxias.dia,&dataHoraMinimaCaxias.hora,&dataHoraMinimaCaxias.minuto,&dataHoraMinimaCaxias.segundo);
    sscanf(argsImprimirDados.estatisticasCaxias.dadosUmidade.dataHoraMaxima, "%d-%d-%dT%d:%d:%d", &dataHoraMaximaCaxias.ano,&dataHoraMaximaCaxias.mes,&dataHoraMaximaCaxias.dia,&dataHoraMaximaCaxias.hora,&dataHoraMaximaCaxias.minuto,&dataHoraMaximaCaxias.segundo);

    sscanf(argsImprimirDados.estatisticasBento.dadosUmidade.dataHoraMinima, "%d-%d-%dT%d:%d:%d", &dataHoraMinimaBento.ano,&dataHoraMinimaBento.mes,&dataHoraMinimaBento.dia,&dataHoraMinimaBento.hora,&dataHoraMinimaBento.minuto,&dataHoraMinimaBento.segundo);
    sscanf(argsImprimirDados.estatisticasBento.dadosUmidade.dataHoraMaxima, "%d-%d-%dT%d:%d:%d", &dataHoraMaximaBento.ano,&dataHoraMaximaBento.mes,&dataHoraMaximaBento.dia,&dataHoraMaximaBento.hora,&dataHoraMaximaBento.minuto,&dataHoraMaximaBento.segundo);

    printf("Caxias do Sul     | %.2lf   | %02d/%02d/%d %02d:%02d:%02d   | %.2lf  | %02d/%02d/%d %02d:%02d:%02d   | %.2lf\n", argsImprimirDados.estatisticasCaxias.dadosUmidade.minima, dataHoraMinimaCaxias.dia, dataHoraMinimaCaxias.mes, dataHoraMinimaCaxias.ano, dataHoraMinimaCaxias.hora, dataHoraMinimaCaxias.minuto, dataHoraMinimaCaxias.segundo, argsImprimirDados.estatisticasCaxias.dadosUmidade.maxima, dataHoraMaximaCaxias.dia, dataHoraMaximaCaxias.mes, dataHoraMaximaCaxias.ano, dataHoraMaximaCaxias.hora, dataHoraMaximaCaxias.minuto, dataHoraMaximaCaxias.segundo, argsImprimirDados.estatisticasCaxias.dadosUmidade.media);
    printf("Bento Gonçalves   | %.2lf   | %02d/%02d/%d %02d:%02d:%02d   | %.2lf  | %02d/%02d/%d %02d:%02d:%02d   | %.2lf\n\n",argsImprimirDados.estatisticasBento.dadosUmidade.minima, dataHoraMinimaBento.dia, dataHoraMinimaBento.mes, dataHoraMinimaBento.ano, dataHoraMinimaBento.hora, dataHoraMinimaBento.minuto, dataHoraMinimaBento.segundo, argsImprimirDados.estatisticasBento.dadosUmidade.maxima, dataHoraMaximaBento.dia, dataHoraMaximaBento.mes, dataHoraMaximaBento.ano, dataHoraMaximaBento.hora, dataHoraMaximaBento.minuto, dataHoraMaximaBento.segundo, argsImprimirDados.estatisticasBento.dadosUmidade.media);
    printf("------------------------------------------------------------\n");
    printf("PRESSÃO ATMOSFÉRICA (hPa)\n");
    printf("------------------------------------------------------------\n");
    printf("Cidade            | Mínima  | Data/Hora             | Máxima | Data/Hora             | Média\n");
    printf("-----------------------------------------------------------------------------------------------\n");

    sscanf(argsImprimirDados.estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMinima, "%d-%d-%dT%d:%d:%d", &dataHoraMinimaCaxias.ano,&dataHoraMinimaCaxias.mes,&dataHoraMinimaCaxias.dia,&dataHoraMinimaCaxias.hora,&dataHoraMinimaCaxias.minuto,&dataHoraMinimaCaxias.segundo);
    sscanf(argsImprimirDados.estatisticasCaxias.dadosPressaoAtmosferica.dataHoraMaxima, "%d-%d-%dT%d:%d:%d", &dataHoraMaximaCaxias.ano,&dataHoraMaximaCaxias.mes,&dataHoraMaximaCaxias.dia,&dataHoraMaximaCaxias.hora,&dataHoraMaximaCaxias.minuto,&dataHoraMaximaCaxias.segundo);

    sscanf(argsImprimirDados.estatisticasBento.dadosPressaoAtmosferica.dataHoraMinima, "%d-%d-%dT%d:%d:%d", &dataHoraMinimaBento.ano,&dataHoraMinimaBento.mes,&dataHoraMinimaBento.dia,&dataHoraMinimaBento.hora,&dataHoraMinimaBento.minuto,&dataHoraMinimaBento.segundo);
    sscanf(argsImprimirDados.estatisticasBento.dadosPressaoAtmosferica.dataHoraMaxima, "%d-%d-%dT%d:%d:%d", &dataHoraMaximaBento.ano,&dataHoraMaximaBento.mes,&dataHoraMaximaBento.dia,&dataHoraMaximaBento.hora,&dataHoraMaximaBento.minuto,&dataHoraMaximaBento.segundo);

    printf("Caxias do Sul     | %.2lf   | %02d/%02d/%d %02d:%02d:%02d   | %.2lf  | %02d/%02d/%d %02d:%02d:%02d   | %.2lf\n", argsImprimirDados.estatisticasCaxias.dadosPressaoAtmosferica.minima, dataHoraMinimaCaxias.dia, dataHoraMinimaCaxias.mes, dataHoraMinimaCaxias.ano, dataHoraMinimaCaxias.hora, dataHoraMinimaCaxias.minuto, dataHoraMinimaCaxias.segundo, argsImprimirDados.estatisticasCaxias.dadosPressaoAtmosferica.maxima, dataHoraMaximaCaxias.dia, dataHoraMaximaCaxias.mes, dataHoraMaximaCaxias.ano, dataHoraMaximaCaxias.hora, dataHoraMaximaCaxias.minuto, dataHoraMaximaCaxias.segundo, argsImprimirDados.estatisticasCaxias.dadosPressaoAtmosferica.media);
    printf("Bento Gonçalves   | %.2lf   | %02d/%02d/%d %02d:%02d:%02d   | %.2lf  | %02d/%02d/%d %02d:%02d:%02d   | %.2lf\n\n", argsImprimirDados.estatisticasBento.dadosPressaoAtmosferica.minima, dataHoraMinimaBento.dia, dataHoraMinimaBento.mes, dataHoraMinimaBento.ano, dataHoraMinimaBento.hora, dataHoraMinimaBento.minuto, dataHoraMinimaBento.segundo, argsImprimirDados.estatisticasBento.dadosPressaoAtmosferica.maxima, dataHoraMaximaBento.dia, dataHoraMaximaBento.mes, dataHoraMaximaBento.ano, dataHoraMaximaBento.hora, dataHoraMaximaBento.minuto, dataHoraMaximaBento.segundo, argsImprimirDados.estatisticasBento.dadosPressaoAtmosferica.media);
    printf("------------------------------------------------------------\n");
    printf("BATERIA\n");
    printf("------------------------------------------------------------\n");
    printf("Cidade            | Inicial (V) | Final (V) | Consumo (V)\n");
    printf("-----------------------------------------------------------------------------------------------\n");
    printf("Caxias do Sul     | %.2lf        | %.2lf      | %.2lf\n", argsImprimirDados.estatisticasCaxias.dadosBateria.inicial, argsImprimirDados.estatisticasCaxias.dadosBateria.final, argsImprimirDados.estatisticasCaxias.dadosBateria.consumo);
    printf("Bento Gonçalves   | %.2lf        | %.2lf      | %.2lf\n\n", argsImprimirDados.estatisticasBento.dadosBateria.inicial, argsImprimirDados.estatisticasBento.dadosBateria.final, argsImprimirDados.estatisticasBento.dadosBateria.consumo);
    printf("------------------------------------------------------------\n");
    printf("SPREADING FACTORS UTILIZADOS\n");
    printf("------------------------------------------------------------\n");
    printf("Cidade            | SF utilizados\n");
    printf("-----------------------------------------------------------------------------------------------\n");
    printf("Caxias do Sul     | ");
    int i,j;
    for (i = 0; i < argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.nItens-1; i++)
    {
        for (j = 0; j < argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.nItens-i-1; j++)
        {
            if(argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j] > argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j+1]){
                argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j] = argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j] ^ argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j+1];
                argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j+1] = argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j] ^ argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j+1];
                argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j] = argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j] ^ argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[j+1];
            }
        }
    }
    for (i = 0; i < argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.nItens; i++)
    {
        printf("SF%d ",argsImprimirDados.estatisticasCaxias.dadosSpreadingFactors.spreadingFactors[i]);
    }
    printf("\n");
    printf("Bento Gonçalves   | ");
    for (i = 0; i < argsImprimirDados.estatisticasBento.dadosSpreadingFactors.nItens-1; i++)
    {
        for (j = 0; j < argsImprimirDados.estatisticasBento.dadosSpreadingFactors.nItens-i-1; j++)
        {
            if(argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j] > argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j+1]){
                argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j] = argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j] ^ argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j+1];
                argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j+1] = argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j] ^ argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j+1];
                argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j] = argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j] ^ argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[j+1];
            }
        }
    }
    for (i = 0; i < argsImprimirDados.estatisticasBento.dadosSpreadingFactors.nItens; i++)
    {
        printf("SF%d ",argsImprimirDados.estatisticasBento.dadosSpreadingFactors.spreadingFactors[i]);
    }
    printf("\n\n");
    printf("------------------------------------------------------------\n");
    printf("DESEMPENHO\n");
    printf("------------------------------------------------------------\n");
    printf("Tempo total de execução: %.2f segundos\n",argsImprimirDados.tempo);
    printf("Threads utilizadas: 5\n");
    printf("- 2 Thread para leitura e parse de JSON(1 para cada arquivo)\n");
    printf("- 2 Thread para cálculo das estatísticas(1 para cada thread de leitura)\n");
    printf("- 1 Thread para registro de logs\n\n");
    printf("Arquivo de log gerado: processamento.log\n\n");
    printf("============================================================\n");
    printf("Processamento finalizado com sucesso.\n");
    printf("============================================================\n");
}

void log_push(LogQueue *lq, const char *mensagem)
{
    ItemLog item;
    item.ultimo = 0;
    strncpy(item.mensagem, mensagem, LOG_MSG_SIZE - 1);
    item.mensagem[LOG_MSG_SIZE - 1] = '\0';

    sem_wait(&lq->sem_empty);
    pthread_mutex_lock(&lq->mutex);
    lq->buffer[lq->head] = item;
    lq->head = (lq->head + 1) % LOG_QUEUE_SIZE;
    lq->count++;
    pthread_mutex_unlock(&lq->mutex);
    sem_post(&lq->sem_full);
}

void *threadLog(void *args)
{
    LogQueue *lq = (LogQueue *)args;
    FILE *f = fopen("processamento.log", "w");
    if (!f) { printf("Erro ao abrir arquivo de log!\n"); return NULL; }

    while (1)
    {
        sem_wait(&lq->sem_full);
        pthread_mutex_lock(&lq->mutex);
        ItemLog item = lq->buffer[lq->tail];
        lq->tail = (lq->tail + 1) % LOG_QUEUE_SIZE;
        lq->count--;
        pthread_mutex_unlock(&lq->mutex);
        sem_post(&lq->sem_empty);

        if (item.ultimo) break;

        fprintf(f, "%s\n", item.mensagem);
        fflush(f); // garante gravação imediata
    }

    fclose(f);
    return NULL;
}
