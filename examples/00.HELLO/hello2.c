#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include "net/rime/collect.h"

#include "dev/leds.h"
#include "dev/button-sensor.h"

#include "net/netstack.h"

#include <stdio.h>

#define MAX_PACKETS 5 // Número máximo de pacotes na fila

static int queue_size = 0; // Número atual de pacotes na fila
static struct queuebuf *packet_queue[MAX_PACKETS]; // Array para armazenar pacotes
static struct ctimer send_timer;

static struct collect_conn tc;

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process2, "Hello world process 2");
AUTOSTART_PROCESSES(&hello_world_process2);
/*---------------------------------------------------------------------------*/



static void recv(const linkaddr_t *originator, uint8_t seqno, uint8_t hops)
{
  if(hops>0){
  	// printf("Sink recebe '%s' de  %d (HOPS %d)\n",
    //      (char *)packetbuf_dataptr(),
    //      originator->u8[0],
    //      hops);
  }
}

/* Função para adicionar um pacote à fila */
void enqueue_packet(const char *data) {
  if (queue_size < MAX_PACKETS) {
      struct queuebuf *qb = queuebuf_new_from_packetbuf();
      if (qb != NULL) {
          packet_queue[queue_size++] = qb; // Adiciona à fila
          // printf("Pacote armazenado na fila (Total: %d)\n", queue_size);
      } else {
          // printf("Erro: Fila cheia ou memória insuficiente!\n");
      }
  } else {
      // printf("Erro: Fila de pacotes está cheia!\n");
  }
}

/* Função para enviar pacotes da fila */
void send_queued_packets(void *ptr) {
  if (queue_size > 0) {
      queuebuf_to_packetbuf(packet_queue[0]); // Carrega o primeiro pacote da fila
      collect_send(&tc, 15); // Envia o pacote pelo protocolo Collect

      // printf("Enviando pacote da fila (Restantes: %d)\n", queue_size - 1);
      
      queuebuf_free(packet_queue[0]); // Libera memória
      int i = 1;
      for (; i < queue_size;) {
        packet_queue[i - 1] = packet_queue[i]; // Shift na fila
        i++;
      }
      queue_size--; // Reduz tamanho da fila

      if (queue_size > 0) {
          ctimer_set(&send_timer, CLOCK_SECOND / 8, send_queued_packets, NULL); // Próximo envio em 0.5s
      }
  }
}


/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = { recv };
/*---------------------------------------------------------------------------*/



PROCESS_THREAD(hello_world_process2, ev, data)
{
  static struct etimer periodic;
  static struct etimer et;


  PROCESS_BEGIN();

  collect_open(&tc, 130, COLLECT_ROUTER, &callbacks);

  if(linkaddr_node_addr.u8[0] == 5 &&
     linkaddr_node_addr.u8[1] == 0) {
    printf("I am gateway\n");
    collect_set_sink(&tc, 0, 1);
  } else if(linkaddr_node_addr.u8[0] == 6 &&
    linkaddr_node_addr.u8[1] == 0) {
    printf("I am sink\n");
    collect_set_sink(&tc, 1, 0); // Configura este nó como sink
  }

  /* Allow some time for the network to settle. */
  etimer_set(&et, 120 * CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&et));

  while(1) {

    /* Send a packet every 30 seconds. */
    const int time = 2;
    etimer_set(&periodic, CLOCK_SECOND * time);
    etimer_set(&et, random_rand() % (CLOCK_SECOND * time));

    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    {
      static linkaddr_t oldparent;
      const linkaddr_t *parent;

      if(!(linkaddr_node_addr.u8[0] == 5 && linkaddr_node_addr.u8[1] == 0)) {
        // Adicionando 5 pacotes à fila
        int i = 0;
        for (;i < 8;) {
          packetbuf_clear();
          packetbuf_set_datalen(sprintf(packetbuf_dataptr(), "Pacote %d", i + 1) + 1);
          enqueue_packet(packetbuf_dataptr());
          i++;
        }
      }

      // printf("Sending\n");
      // packetbuf_clear();
      // packetbuf_set_datalen(sprintf(packetbuf_dataptr(),
      //                               "%d", 24) + 1);
      // collect_send(&tc, 15);
      // collect_send(&tc, 15);

      ctimer_set(&send_timer, CLOCK_SECOND / 2, send_queued_packets, NULL);

      parent = collect_parent(&tc);
      if(!linkaddr_cmp(parent, &oldparent)) {
        if(!linkaddr_cmp(&oldparent, &linkaddr_null)) {
          printf("#L %d 0\n", oldparent.u8[0]);
        }
        if(!linkaddr_cmp(parent, &linkaddr_null)) {
          printf("#L %d 1\n", parent->u8[0]);
        }
        linkaddr_copy(&oldparent, parent);
      }
    }

    PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/