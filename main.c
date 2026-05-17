#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include "structs.h"
#include "processData.h"
#include <pthread.h>   
#include <semaphore.h> 

//Implementar função de gera logs

int main(){
    struct timespec inicio, fim;
    clock_gettime(CLOCK_MONOTONIC, &inicio); //Forma mais consistente de medir o tempo, essa linha fica com erro no vscode mas funciona quando builda

    //inicializa Fila de logs
    LogQueue lq;
    lq.head = lq.tail = lq.count = 0;
    pthread_mutex_init(&lq.mutex, NULL);
    sem_init(&lq.sem_empty, 0, LOG_QUEUE_SIZE);
    sem_init(&lq.sem_full,  0, 0);

    // inicializa SharedBuffer 1
    SharedBuffer sb1;
    sb1.head = sb1.tail = sb1.count = 0;
    sb1.registros_lidos = sb1.registros_processados = sb1.leitura_concluida = sb1.total_registros = sb1.registros_duplicados = 0;
    pthread_mutex_init(&sb1.mutex, NULL);
    sem_init(&sb1.sem_empty, 0, BUFFER_SIZE);
    sem_init(&sb1.sem_full,  0, 0);

    // inicializa SharedBuffer 2
    SharedBuffer sb2;
    sb2.head = sb2.tail = sb2.count = 0;
    sb2.registros_lidos = sb2.registros_processados = sb2.leitura_concluida = sb2.total_registros = sb2.registros_duplicados = 0;
    pthread_mutex_init(&sb2.mutex, NULL);
    sem_init(&sb2.sem_empty, 0, BUFFER_SIZE);
    sem_init(&sb2.sem_full,  0, 0);

    // args de leitura
    ARGSCARREGARJSON argsCarga1 = {"senzemo_cx_bg.json",    NULL, &sb1, &lq};
    ARGSCARREGARJSON argsCarga2 = {"mqtt_senzemo_cx_bg.json", NULL, &sb2, &lq};

    // inicializa estatísticas
    DADOSTEMPERATURA dadosTemperatura = {INT_MIN, "", INT_MAX, "", 0};
    DADOSUMIDADE dadosUmidade = {INT_MIN, "", INT_MAX, "", 0};
    DADOSPRESSAOTMOSFERICA dadosPressaoAtmosferica = {INT_MIN, "", INT_MAX, "", 0};
    DADOSBATERIA dadosBateria = {0, 10, 0};
    DADOSSPREADINGFACTOR dadosSpreadingFactors = {{0}, 0};
    CONTADORES contadores = {0, 0, 0};

    ESTATISTICASCAXIAS estatisticasCaxias = {0, dadosTemperatura, dadosUmidade, dadosPressaoAtmosferica, dadosBateria, dadosSpreadingFactors, contadores};
    ESTATISTICASBENTO  estatisticasBento  = {0, dadosTemperatura, dadosUmidade, dadosPressaoAtmosferica, dadosBateria, dadosSpreadingFactors, contadores};

    // args de processamento
    ARGSPROCESSARJSON argsProc1;
    argsProc1.shared = &sb1;
    argsProc1.estatisticasCaxias = estatisticasCaxias;
    argsProc1.estatisticasBento  = estatisticasBento;
    argsProc1.lq = &lq;

    ARGSPROCESSARJSONMQTT argsProc2;
    argsProc2.shared = &sb2;
    argsProc2.estatisticasCaxias = estatisticasCaxias;
    argsProc2.estatisticasBento  = estatisticasBento;
    argsProc2.lq = &lq;

    pthread_t tLog;
    pthread_create(&tLog, NULL, threadLog, &lq);

    // dispara os dois pipelines em paralelo
    pthread_t tLeitura1, tLeitura2, tProcessar1, tProcessar2;
    pthread_create(&tLeitura1,   NULL, carregarJson,      &argsCarga1);
    pthread_create(&tProcessar1, NULL, processarJson,     &argsProc1);
    pthread_create(&tLeitura2,   NULL, carregarJson,      &argsCarga2);
    pthread_create(&tProcessar2, NULL, processarJsonMqtt, &argsProc2);

    pthread_join(tLeitura1,   NULL);
    pthread_join(tLeitura2,   NULL);
    pthread_join(tProcessar1, NULL);
    pthread_join(tProcessar2, NULL);


    // após pthread_join das 4 threads, envia sentinela pra thread de log
    sem_wait(&lq.sem_empty);
    pthread_mutex_lock(&lq.mutex);
    ItemLog fimLog = {0};
    fimLog.ultimo = 1;
    lq.buffer[lq.head] = fimLog;
    lq.head = (lq.head + 1) % LOG_QUEUE_SIZE;
    lq.count++;
    pthread_mutex_unlock(&lq.mutex);
    sem_post(&lq.sem_full);

    pthread_join(tLog, NULL);

    // libera recursos
    pthread_mutex_destroy(&sb1.mutex);
    sem_destroy(&sb1.sem_empty);
    sem_destroy(&sb1.sem_full);
    pthread_mutex_destroy(&sb2.mutex);
    sem_destroy(&sb2.sem_empty);
    sem_destroy(&sb2.sem_full);


    // libera recursos da fila de log
    pthread_mutex_destroy(&lq.mutex);
    sem_destroy(&lq.sem_empty);
    sem_destroy(&lq.sem_full);

    // merge dos resultados
    ESTATISTICASCAXIAS estatisticasCaxiasFinal = calcularEstatisticasCaxias(argsProc1.estatisticasCaxias, argsProc2.estatisticasCaxias);
    ESTATISTICASBENTO  estatisticasBentoFinal  = calcularEstatisticasBento(argsProc1.estatisticasBento,  argsProc2.estatisticasBento);

   clock_gettime(CLOCK_MONOTONIC, &fim);
    float segundos = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    DATAHORA dataInicioArquivo1, dataFimArquivo1, dataInicioArquivo2, dataFimArquivo2;

    sscanf(argsProc1.periodoInicio, "%d-%d-%d", &dataInicioArquivo1.ano, &dataInicioArquivo1.mes, &dataInicioArquivo1.dia);
    sscanf(argsProc1.periodoFim,    "%d-%d-%d", &dataFimArquivo1.ano,    &dataFimArquivo1.mes,    &dataFimArquivo1.dia);

    sscanf(argsProc2.periodoInicio, "%d-%d-%d", &dataInicioArquivo2.ano, &dataInicioArquivo2.mes, &dataInicioArquivo2.dia);
    sscanf(argsProc2.periodoFim,    "%d-%d-%d", &dataFimArquivo2.ano,    &dataFimArquivo2.mes,    &dataFimArquivo2.dia);

    ARGSIMPRIMIRDADOS argsImprimirDados = {
        "senzemo_cx_bg.json",
        argsProc1.estatisticasCaxias.numeroRegistros + argsProc1.estatisticasBento.numeroRegistros,
        sb1.registros_duplicados,
        dataInicioArquivo1,
        dataFimArquivo1,
        "mqtt_senzemo_cx_bg.json",
        argsProc2.estatisticasCaxias.numeroRegistros + argsProc2.estatisticasBento.numeroRegistros,
        sb2.registros_duplicados,
        dataInicioArquivo2,
        dataFimArquivo2,
        segundos,
        estatisticasCaxiasFinal,
        estatisticasBentoFinal
    };
    //Impressão dos dados na tela
    imprimirInformacoesNaTela(argsImprimirDados);

    return 0;
}
