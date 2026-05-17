#ifndef PROCESSDATA_H
#define PROCESSDATA_H

void *carregarJson(void *args);
void *processarJson(void *args);
void *processarJsonMqtt(void *args);
ESTATISTICASCAXIAS calcularEstatisticasCaxias(ESTATISTICASCAXIAS estatisticasCaxias1, ESTATISTICASCAXIAS estatisticasCaxias2, LogQueue *lq);
ESTATISTICASBENTO calcularEstatisticasBento(ESTATISTICASBENTO estatisticasBento1, ESTATISTICASBENTO estatisticasBento2, LogQueue *lq);
void imprimirInformacoesNaTela(ARGSIMPRIMIRDADOS argsImprimirDados);
void *threadLog(void *args);
void log_push(LogQueue *lq, const char *mensagem);
#endif